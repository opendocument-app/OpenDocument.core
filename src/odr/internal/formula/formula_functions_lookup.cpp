#include <odr/internal/formula/formula_function.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_text.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace odr::internal::formula {

namespace {

/// @p value with a boolean as the number LibreOffice states it as.
Value as_dialect(const Call &call, const Value &value) {
  if (const auto *boolean = std::get_if<bool>(&value.content);
      boolean != nullptr && is_libreoffice(call)) {
    return Value{*boolean ? 1.0 : 0.0};
  }
  return value;
}

/// Whether two texts are one without case, as a criterion and a lookup
/// compare them in both applications, whatever the settings.
bool same_text(const std::string_view a, const std::string_view b) {
  return equal_texts(a, b, false);
}

/// A rectangle of cells by position, read row by row.
class Cells final {
public:
  Cells(const Call &call, const Value &value) : m_call{&call} {
    if (const auto *reference = std::get_if<Reference>(&value.content)) {
      if (reference->areas.size() != 1) {
        throw NoAnswer{};
      }
      m_area = reference->areas.front();
      whole = m_area->whole_columns || m_area->whole_rows;
      const auto width = std::uint64_t{m_area->range.to().column} -
                         m_area->range.from().column + 1;
      const auto height =
          std::uint64_t{m_area->range.to().row} - m_area->range.from().row + 1;
      if (width > std::numeric_limits<std::uint32_t>::max() ||
          height > std::numeric_limits<std::uint32_t>::max()) {
        throw NoAnswer{};
      }
      columns = static_cast<std::uint32_t>(width);
      rows = static_cast<std::uint32_t>(height);
    } else if (const auto *matrix = std::get_if<Matrix>(&value.content)) {
      m_matrix = *matrix;
      columns = matrix->columns;
      rows = matrix->rows;
    } else if (value.holds<ErrorType>()) {
      throw ErrorResult{value.get<ErrorType>()};
    } else {
      m_matrix = Matrix{1, 1, {value}};
      columns = 1;
      rows = 1;
    }
  }

  std::uint32_t columns{0};
  std::uint32_t rows{0};
  /// Whether the cells are a whole column or row, which the extent of its
  /// sheet cuts and the grid of an application does not.
  bool whole{false};

  [[nodiscard]] Value at(const std::uint32_t column,
                         const std::uint32_t row) const {
    if (m_area.has_value()) {
      return m_call->cell_value(
          SheetPosition(m_area->sheet, m_area->range.from().column + column,
                        m_area->range.from().row + row));
    }
    return m_matrix.cells[std::size_t{row} * m_matrix.columns + column];
  }

  /// The reference to the cell at (@p column, @p row), or its value where
  /// the cells are an array.
  [[nodiscard]] Value reference_at(const std::uint32_t column,
                                   const std::uint32_t row) const {
    if (!m_area.has_value()) {
      return at(column, row);
    }
    const TablePosition position(m_area->range.from().column + column,
                                 m_area->range.from().row + row);
    return Value{
        Reference{{Area{m_area->sheet, TableRange(position, position)}}}};
  }

private:
  const Call *m_call{nullptr};
  std::optional<Area> m_area;
  Matrix m_matrix;
};

/// What a criterion compares a cell with.
enum class Relation {
  equal,
  not_equal,
  less,
  less_equal,
  greater,
  greater_equal
};

/// A criterion of `COUNTIF` and its family: `"Sp"`, `">4"`, `"<>"`, `5`.
/// Both applications match a text without case. A criterion whose meaning
/// depends on what they do not agree on, or on a pattern, has no answer.
class Criterion final {
public:
  Criterion(const Call &call, const Value &stated) : m_call{&call} {
    const Value value = as_dialect(call, stated);
    if (const auto *error = std::get_if<ErrorType>(&value.content)) {
      throw ErrorResult{*error};
    }
    if (const auto *number = std::get_if<double>(&value.content)) {
      m_number = *number;
      m_exact_number = true;
      return;
    }
    const auto *text = std::get_if<std::string>(&value.content);
    if (text == nullptr) {
      throw NoAnswer{}; // an empty cell, or a boolean in Excel
    }
    std::string_view rest = *text;
    for (const auto &[spelling, relation] :
         {std::pair<std::string_view, Relation>{"<=", Relation::less_equal},
          {">=", Relation::greater_equal},
          {"<>", Relation::not_equal},
          {"<", Relation::less},
          {">", Relation::greater},
          {"=", Relation::equal}}) {
      if (rest.starts_with(spelling)) {
        m_relation = relation;
        m_stated_relation = true;
        rest.remove_prefix(spelling.size());
        break;
      }
    }
    if (rest.empty()) {
      if (m_relation != Relation::equal && m_relation != Relation::not_equal) {
        throw NoAnswer{};
      }
      m_blank = true;
      return;
    }
    if (const std::optional<Number> number = number_of_text(rest);
        number.has_value() && std::holds_alternative<double>(*number)) {
      m_number = std::get<double>(*number);
      return;
    }
    if (is_truth_text(rest) || is_pattern(call, rest) ||
        !call.settings().whole_cell) {
      throw NoAnswer{};
    }
    m_text = std::string(rest);
  }

  [[nodiscard]] bool matches(const Value &stated) const {
    const Value value = as_dialect(*m_call, stated);
    if (m_blank) {
      // `""` matches an empty cell and an empty text, `"="` an empty cell
      const bool empty = value.holds<Empty>();
      const bool empty_text =
          value.holds<std::string>() && value.get<std::string>().empty();
      if (m_relation == Relation::not_equal) {
        return !empty;
      }
      return empty || (empty_text && !m_stated_relation);
    }
    if (value.holds<ErrorType>()) {
      if (m_relation == Relation::not_equal) {
        throw NoAnswer{};
      }
      return false;
    }
    const std::optional<std::strong_ordering> order = order_with(value);
    if (!order.has_value()) {
      return m_relation == Relation::not_equal;
    }
    switch (m_relation) {
    case Relation::equal:
      return *order == 0;
    case Relation::not_equal:
      return *order != 0;
    case Relation::less:
      return *order < 0;
    case Relation::less_equal:
      return *order <= 0;
    case Relation::greater:
      return *order > 0;
    case Relation::greater_equal:
      return *order >= 0;
    }
    return false;
  }

private:
  const Call *m_call{nullptr};
  Relation m_relation{Relation::equal};
  bool m_stated_relation{false};
  bool m_blank{false};
  std::optional<double> m_number;
  /// Whether the criterion is a number, not a text spelling one: `5`, which
  /// matches no text, against `"5"`, which matches the text `5` too.
  bool m_exact_number{false};
  std::string m_text;

  static bool is_truth_text(const std::string_view text) {
    return same_text(text, "TRUE") || same_text(text, "FALSE");
  }

  /// How @p value orders against the criterion, nothing where the two do not
  /// compare: a number and a text.
  [[nodiscard]] std::optional<std::strong_ordering>
  order_with(const Value &value) const {
    if (m_number.has_value()) {
      if (const auto *number = std::get_if<double>(&value.content)) {
        if (same_number(m_call->settings().dialect, *number, *m_number)) {
          return std::strong_ordering::equal;
        }
        return *number < *m_number ? std::strong_ordering::less
                                   : std::strong_ordering::greater;
      }
      const auto *text = std::get_if<std::string>(&value.content);
      if (text == nullptr || m_exact_number) {
        if (text != nullptr && !is_libreoffice(*m_call)) {
          // whether Excel reads a text as the number it spells here is not
          // documented
          const std::optional<Number> read = number_of_text(*text);
          if (read.has_value() && std::holds_alternative<double>(*read)) {
            throw NoAnswer{};
          }
        }
        return std::nullopt;
      }
      // a text spelling the number is the number for `=` and `<>`
      const std::optional<Number> read = number_of_text(*text);
      if (!read.has_value() && std::ranges::any_of(*text, [](const char c) {
            return c >= '0' && c <= '9';
          })) {
        throw NoAnswer{};
      }
      if (m_relation != Relation::equal && m_relation != Relation::not_equal) {
        return std::nullopt;
      }
      if (read.has_value() && std::holds_alternative<double>(*read) &&
          same_number(m_call->settings().dialect, std::get<double>(*read),
                      *m_number)) {
        return std::strong_ordering::equal;
      }
      return std::nullopt;
    }
    const auto *text = std::get_if<std::string>(&value.content);
    if (text == nullptr) {
      return std::nullopt;
    }
    if (m_relation == Relation::equal || m_relation == Relation::not_equal) {
      return same_text(*text, m_text) ? std::strong_ordering::equal
                                      : std::strong_ordering::less;
    }
    return order_of_texts(*text, m_text, false);
  }
};

/// The criteria pairs of `COUNTIFS`, `SUMIFS` and `AVERAGEIFS` from
/// argument @p first on, each a range and a criterion.
struct Condition final {
  Cells cells;
  Criterion criterion;
};

std::vector<Condition> conditions(const Call &call, const std::size_t first) {
  if (call.size() <= first || (call.size() - first) % 2 != 0) {
    throw NoAnswer{};
  }
  std::vector<Condition> result;
  for (std::size_t i = first; i < call.size(); i += 2) {
    if (call.missing(i) || call.missing(i + 1)) {
      throw NoAnswer{};
    }
    result.push_back(Condition{Cells(call, call.value(i)),
                               Criterion(call, call.scalar(i + 1))});
    if (result.back().cells.columns != result.front().cells.columns ||
        result.back().cells.rows != result.front().cells.rows) {
      // whole columns of two sheets differ by the extents, not the grid
      if (is_libreoffice(call) || result.back().cells.whole ||
          result.front().cells.whole) {
        throw NoAnswer{};
      }
      throw ErrorResult{ErrorType::value};
    }
  }
  return result;
}

/// The most positions the conditions of one call test.
constexpr std::size_t cell_limit = 1 << 22;

/// Visits cells matching all @p conditions. Refuses unbounded ranges when
/// empty cells beyond the sheet extent would also match.
template <typename Visit>
void each_match(const std::span<const Condition> conditions,
                const Visit &visit) {
  const Cells &shape = conditions.front().cells;
  if (std::uint64_t{shape.columns} * shape.rows > cell_limit) {
    throw NoAnswer{};
  }
  if (std::ranges::any_of(conditions,
                          [](const Condition &c) { return c.cells.whole; }) &&
      std::ranges::all_of(conditions, [](const Condition &c) {
        return c.criterion.matches(Value{Empty{}});
      })) {
    throw NoAnswer{};
  }
  for (std::uint32_t row = 0; row < shape.rows; ++row) {
    for (std::uint32_t column = 0; column < shape.columns; ++column) {
      bool all = true;
      for (const Condition &condition : conditions) {
        if (!condition.criterion.matches(condition.cells.at(column, row))) {
          all = false;
          break;
        }
      }
      if (all) {
        visit(column, row);
      }
    }
  }
}

/// `COUNTIF` and `COUNTIFS`, which take up to @p most arguments.
template <std::size_t most> Value count_if(const Call &call) {
  expect_arguments(call, 2, most);
  double result = 0;
  each_match(conditions(call, 0),
             [&](std::uint32_t, std::uint32_t) { ++result; });
  return Value{result};
}

/// The sum of @p values at the positions that meet @p conditions, or their
/// average where @p average. A text and an empty cell add nothing, an error
/// is the result.
Value matched(const Call &call, const std::span<const Condition> conditions,
              const Cells &values, const bool average) {
  std::vector<double> numbers;
  Errors errors;
  each_match(
      conditions, [&](const std::uint32_t column, const std::uint32_t row) {
        const Value value = as_dialect(call, values.at(column, row));
        if (const auto *number = std::get_if<double>(&value.content)) {
          numbers.push_back(*number);
        } else if (const auto *error = std::get_if<ErrorType>(&value.content)) {
          errors.add(*error);
        }
      });
  if (const auto error = errors.first(); error.has_value()) {
    throw ErrorResult{*error};
  }
  if (average && numbers.empty()) {
    return Value{ErrorType::division};
  }
  Value sum = sum_of(call, numbers);
  if (!average || !sum.holds<double>()) {
    return sum;
  }
  return Value{sum.get<double>() / static_cast<double>(numbers.size())};
}

/// The range a `SUMIF` adds: the one it names, of the size of the range it
/// tests, from its first cell on, as both applications read it.
Cells sum_range(const Call &call, const Cells &tested,
                const std::size_t index) {
  if (index >= call.size()) {
    return tested;
  }
  if (call.missing(index)) {
    throw NoAnswer{};
  }
  const Cells stated(call, call.value(index));
  if (stated.columns != tested.columns || stated.rows != tested.rows) {
    throw NoAnswer{};
  }
  return stated;
}

template <bool average> Value sum_if(const Call &call) {
  expect_arguments(call, 2, 3);
  const std::vector<Condition> tested{
      Condition{Cells(call, call.value(0)), Criterion(call, call.scalar(1))}};
  const Cells values = sum_range(call, tested.front().cells, 2);
  return matched(call, tested, values, average);
}

template <bool average> Value sum_ifs(const Call &call) {
  expect_arguments(call, 3, 255);
  const std::vector<Condition> tested = conditions(call, 1);
  const Cells values(call, call.value(0));
  if (values.columns != tested.front().cells.columns ||
      values.rows != tested.front().cells.rows) {
    if (values.whole || tested.front().cells.whole) {
      throw NoAnswer{};
    }
    return refused(call, ErrorType::value);
  }
  return matched(call, tested, values, average);
}

/// Whether a cell is the value a lookup asks for: a number the same number,
/// a text the same text without case. A pattern has no answer.
bool is_lookup_match(const Call &call, const Value &wanted,
                     const Value &stated) {
  const Value value = as_dialect(call, stated);
  if (const auto *number = std::get_if<double>(&wanted.content)) {
    return value.holds<double>() &&
           same_number(call.settings().dialect, value.get<double>(), *number);
  }
  if (const auto *text = std::get_if<std::string>(&wanted.content)) {
    return value.holds<std::string>() &&
           same_text(value.get<std::string>(), *text);
  }
  if (const auto *boolean = std::get_if<bool>(&wanted.content)) {
    return value.holds<bool>() && value.get<bool>() == *boolean;
  }
  throw NoAnswer{};
}

/// The value a lookup asks for, as a scalar.
Value wanted_of(const Call &call, const std::size_t index) {
  const Value wanted = as_dialect(call, call.scalar(index));
  if (const auto *error = std::get_if<ErrorType>(&wanted.content)) {
    throw ErrorResult{*error};
  }
  if (wanted.holds<Empty>()) {
    throw NoAnswer{};
  }
  if (const auto *text = std::get_if<std::string>(&wanted.content);
      text != nullptr && is_pattern(call, *text)) {
    throw NoAnswer{};
  }
  return wanted;
}

/// Finds the first exact match, or the last number <= @p wanted. Approximate
/// lookup requires ascending numbers and a unique match; other cases depend
/// on application-specific binary search.
template <typename CellAt>
std::optional<std::uint32_t>
position_of(const Call &call, const Value &wanted, const std::uint32_t count,
            const bool exact, const CellAt &cell_at) {
  if (count > cell_limit) {
    throw NoAnswer{};
  }
  if (exact) {
    for (std::uint32_t i = 0; i < count; ++i) {
      if (is_lookup_match(call, wanted, cell_at(i))) {
        return i;
      }
    }
    return std::nullopt;
  }
  const auto *number = std::get_if<double>(&wanted.content);
  if (number == nullptr) {
    throw NoAnswer{};
  }
  std::optional<std::uint32_t> found;
  std::optional<double> previous;
  for (std::uint32_t i = 0; i < count; ++i) {
    const Value value = as_dialect(call, cell_at(i));
    const auto *cell = std::get_if<double>(&value.content);
    if (cell == nullptr || (previous.has_value() && *cell < *previous)) {
      throw NoAnswer{};
    }
    if (previous.has_value() && *cell == *previous &&
        found == static_cast<std::uint32_t>(i - 1)) {
      throw NoAnswer{};
    }
    if (*cell <= *number ||
        same_number(call.settings().dialect, *cell, *number)) {
      found = i;
    }
    previous = *cell;
  }
  return found;
}

/// Whether a lookup is exact, as its last argument states: 0 or false.
bool exact_of(const Call &call, const std::size_t index, const bool unstated) {
  if (index >= call.size()) {
    return unstated;
  }
  return number_argument(call, index) == 0;
}

/// `VLOOKUP` and `HLOOKUP`: the value in line @p index of the table, of the
/// first column or row matching.
template <bool vertical> Value lookup(const Call &call) {
  expect_arguments(call, 3, 4);
  const Value wanted = wanted_of(call, 0);
  const Cells table(call, call.value(1));
  const double index = std::trunc(number_argument(call, 2));
  const std::uint32_t lines = vertical ? table.columns : table.rows;
  if (index < 1) {
    return refused(call, ErrorType::value);
  }
  if (index > lines) {
    return refused(call, ErrorType::reference);
  }
  const std::uint32_t count = vertical ? table.rows : table.columns;
  const bool exact = exact_of(call, 3, false);
  // an approximate lookup's search meets the empty cells past the extent
  if (!exact && table.whole) {
    throw NoAnswer{};
  }
  const std::optional<std::uint32_t> found =
      position_of(call, wanted, count, exact, [&](const std::uint32_t i) {
        return vertical ? table.at(0, i) : table.at(i, 0);
      });
  if (!found.has_value()) {
    return Value{ErrorType::not_available};
  }
  const auto line = static_cast<std::uint32_t>(index) - 1;
  return vertical ? table.at(line, *found) : table.at(*found, line);
}

/// `MATCH`: the position of a value in a row or a column, counted from 1.
Value match(const Call &call) {
  expect_arguments(call, 2, 3);
  const Value wanted = wanted_of(call, 0);
  const Cells line(call, call.value(1));
  if (line.columns != 1 && line.rows != 1) {
    return Value{ErrorType::not_available};
  }
  const double type = call.size() > 2 ? number_argument(call, 2) : 1;
  if (type < 0) {
    throw NoAnswer{}; // descending order
  }
  if (type != 0 && line.whole) {
    throw NoAnswer{};
  }
  const bool vertical = line.columns == 1;
  const std::optional<std::uint32_t> found =
      position_of(call, wanted, vertical ? line.rows : line.columns, type == 0,
                  [&](const std::uint32_t i) {
                    return vertical ? line.at(0, i) : line.at(i, 0);
                  });
  if (!found.has_value()) {
    return Value{ErrorType::not_available};
  }
  return Value{static_cast<double>(*found + 1)};
}

/// `INDEX`: the cell at a row and a column of a range, counted from 1. A
/// single row or column takes one index.
Value index(const Call &call) {
  expect_arguments(call, 2, 3);
  const Cells cells(call, call.value(0));
  const auto read = [&](const std::size_t i) {
    return std::trunc(number_argument(call, i));
  };
  double row = read(1);
  double column = call.size() > 2 ? read(2) : 1;
  if (call.size() == 2 && cells.rows == 1 && cells.columns > 1) {
    column = row;
    row = 1;
  } else if (call.size() == 2 && cells.columns > 1) {
    throw NoAnswer{}; // a whole row of a table
  }
  if (row == 0 || column == 0) {
    throw NoAnswer{}; // a whole row or column
  }
  if (row < 0 || column < 0) {
    return refused(call, ErrorType::value);
  }
  if (row > cells.rows || column > cells.columns) {
    if (cells.whole) {
      throw NoAnswer{}; // past the extent, not the grid
    }
    return Value{ErrorType::reference};
  }
  return cells.reference_at(static_cast<std::uint32_t>(column) - 1,
                            static_cast<std::uint32_t>(row) - 1);
}

/// The grid of Excel. LibreOffice refuses a reference past its own grid with
/// a code a file cannot hold, so the size of its grid decides nothing here.
constexpr double grid_columns = 16384;
constexpr double grid_rows = 1048576;

/// `OFFSET`: a reference moved by rows and columns, and sized anew where a
/// height or a width is given.
Value offset(const Call &call) {
  expect_arguments(call, 3, 5);
  const Value value = call.value(0);
  if (const auto *error = std::get_if<ErrorType>(&value.content)) {
    return Value{*error};
  }
  const auto *reference = std::get_if<Reference>(&value.content);
  if (reference == nullptr || reference->areas.size() != 1) {
    throw NoAnswer{};
  }
  const Area &area = reference->areas.front();
  if ((area.whole_columns &&
       (call.size() > 3 || number_argument(call, 1) != 0)) ||
      (area.whole_rows && (call.size() > 4 || number_argument(call, 2) != 0))) {
    throw NoAnswer{}; // a whole row or column moved or sized
  }
  const auto read = [&](const std::size_t i) {
    return std::trunc(number_argument(call, i));
  };
  const TablePosition &from = area.range.from();
  const TablePosition &to = area.range.to();
  const double top = from.row + read(1);
  const double left = from.column + read(2);
  const double height = call.size() > 3 ? read(3) : to.row - from.row + 1.0;
  const double width =
      call.size() > 4 ? read(4) : to.column - from.column + 1.0;
  if (height < 1 || width < 1) {
    throw NoAnswer{}; // Excel does not document it, LibreOffice refuses it
  }
  if (top < 0 || left < 0 || top + height > grid_rows ||
      left + width > grid_columns) {
    return refused(call, ErrorType::reference);
  }
  return Value{Reference{
      {Area{area.sheet,
            TableRange({static_cast<std::uint32_t>(left),
                        static_cast<std::uint32_t>(top)},
                       {static_cast<std::uint32_t>(left + width - 1),
                        static_cast<std::uint32_t>(top + height - 1)}),
            area.whole_columns, area.whole_rows}}}};
}

/// One axis of an R1C1 cell: `R2` is row 2, `R[-1]` the row above @p at, and
/// a bare `R` the row of @p at. Advances @p text past it.
std::optional<Coordinate> r1c1_axis(std::string_view &text, const char letter,
                                    const std::uint32_t at,
                                    const double limit) {
  if (text.empty() || util::string::to_upper(text.front()) != letter) {
    return std::nullopt;
  }
  text.remove_prefix(1);
  const bool relative = !text.empty() && text.front() == '[';
  if (relative) {
    text.remove_prefix(1);
  }
  const std::size_t sign = !text.empty() && text.front() == '-' ? 1 : 0;
  std::size_t digits = sign;
  while (digits < text.size() && text[digits] >= '0' && text[digits] <= '9') {
    ++digits;
  }
  double number = 0;
  for (std::size_t i = sign; i < digits; ++i) {
    number = number * 10 + (text[i] - '0');
  }
  if (sign != 0) {
    number = -number;
  }
  const bool stated = digits > sign;
  text.remove_prefix(digits);
  if (relative) {
    if (!stated || text.empty() || text.front() != ']') {
      return std::nullopt;
    }
    text.remove_prefix(1);
  } else if (sign != 0) {
    return std::nullopt;
  }
  const double index = relative || !stated ? at + number : number - 1;
  if (index < 0 || index >= limit) {
    return std::nullopt;
  }
  return Coordinate{static_cast<std::uint32_t>(index), !relative && stated};
}

/// A cell or a range in R1C1, as `INDIRECT` reads one with its second
/// argument false: `Sheet1!R2C3`, `R[-1]C:R[1]C`. Null for anything else,
/// such as a whole row (`R2`), which this does not read.
std::optional<Node> r1c1_reference(std::string_view text,
                                   const TablePosition &at) {
  std::optional<std::string> sheet;
  if (const std::size_t bang = text.rfind('!');
      bang != std::string_view::npos) {
    std::string_view name = text.substr(0, bang);
    if (name.size() >= 2 && name.front() == '\'' && name.back() == '\'') {
      name = name.substr(1, name.size() - 2);
    }
    std::string unquoted(name);
    util::string::replace_all(unquoted, "''", "'");
    sheet = std::move(unquoted);
    text.remove_prefix(bang + 1);
  }
  const auto cell = [&]() -> std::optional<CellReference> {
    const std::optional<Coordinate> row =
        r1c1_axis(text, 'R', at.row, grid_rows);
    const std::optional<Coordinate> column =
        row.has_value() ? r1c1_axis(text, 'C', at.column, grid_columns)
                        : std::nullopt;
    if (!column.has_value()) {
      return std::nullopt;
    }
    return CellReference{std::nullopt, sheet, false, column, row};
  };
  const std::optional<CellReference> from = cell();
  if (!from.has_value()) {
    return std::nullopt;
  }
  if (text.empty()) {
    return Node{*from, {}};
  }
  if (text.front() != ':') {
    return std::nullopt;
  }
  text.remove_prefix(1);
  std::optional<CellReference> to = cell();
  if (!to.has_value() || !text.empty()) {
    return std::nullopt;
  }
  to->sheet = std::nullopt;
  return Node{RangeReference{*from, *to}, {}};
}

/// A reference in A1 as `INDIRECT` reads one: Excel's `Sheet1!A1:B2` and a
/// name, and in LibreOffice its own `$Sheet1.$A$1:B2` too.
std::optional<Node> a1_reference(const std::string_view text,
                                 const bool libreoffice) {
  const auto is_reference = [](const std::optional<Node> &node) {
    return node.has_value() &&
           (node->holds<CellReference>() || node->holds<RangeReference>() ||
            node->holds<NameReference>() ||
            (node->holds<BinaryOperation>() &&
             std::get<BinaryOperation>(node->content).op ==
                 BinaryOperator::range));
  };
  if (text.empty() || text.front() == '=') {
    return std::nullopt;
  }
  // a `.` without a `!` is LibreOffice's sheet separator, and in Excel a name
  const bool calc = libreoffice && text.find('.') != std::string_view::npos &&
                    text.find('!') == std::string_view::npos;
  std::optional<Node> node;
  if (!calc) {
    node = parse(text, Syntax::ooxml);
    if (is_reference(node)) {
      return node;
    }
  }
  if (!libreoffice) {
    return std::nullopt;
  }
  // `Sheet1.A1:B2` is `[Sheet1.A1:.B2]` in a formula
  std::string bracketed = "[" + std::string(text) + "]";
  if (const std::size_t colon = bracketed.find(':');
      colon != std::string::npos && bracketed[colon + 1] != '.' &&
      bracketed.find('.', colon) == std::string::npos) {
    bracketed.insert(colon + 1, ".");
  }
  if (bracketed[1] != '.' && bracketed.find('.') == std::string::npos) {
    bracketed.insert(1, ".");
  }
  node = parse(bracketed, Syntax::opendocument);
  return is_reference(node) ? node : std::nullopt;
}

/// Whether @p text has white space outside a quoted sheet name.
bool spaced(const std::string_view text) {
  bool quoted = false;
  for (const char c : text) {
    if (c == '\'') {
      quoted = !quoted;
    } else if (!quoted && (c == ' ' || c == '\t' || c == '\n')) {
      return true;
    }
  }
  return false;
}

/// `INDIRECT`: the reference a text spells, in A1, or in R1C1 where the
/// second argument is false. A text that spells none is `#REF!`.
Value indirect(const Call &call) {
  expect_arguments(call, 1, 2);
  const Value argument = call.scalar(0);
  if (const auto *error = std::get_if<ErrorType>(&argument.content)) {
    return Value{*error};
  }
  const auto *text = std::get_if<std::string>(&argument.content);
  if (text == nullptr) {
    return Value{ErrorType::reference};
  }
  if (spaced(*text)) {
    if (is_libreoffice(call)) {
      return Value{ErrorType::reference};
    }
    throw NoAnswer{}; // whether Excel trims is not documented
  }
  bool a1 = true;
  if (call.size() > 1) {
    const Value flag = call.scalar(1);
    if (const auto *error = std::get_if<ErrorType>(&flag.content)) {
      return Value{*error};
    }
    if (const auto *boolean = std::get_if<bool>(&flag.content)) {
      a1 = *boolean;
    } else if (const auto *number = std::get_if<double>(&flag.content)) {
      a1 = *number != 0;
    } else {
      throw NoAnswer{};
    }
  }
  if (a1) {
    const std::optional<Node> node = a1_reference(*text, is_libreoffice(call));
    return node.has_value() ? call.reference(*node)
                            : Value{ErrorType::reference};
  }
  const std::optional<Node> node = r1c1_reference(*text, call.cell().cell);
  if (!node.has_value()) {
    throw NoAnswer{}; // a whole row or column, which this does not read
  }
  return call.reference(*node);
}

/// `ROW` and `COLUMN`: of the first cell of a reference, or of the formula's
/// own cell.
template <bool of_row> Value position(const Call &call) {
  if (call.size() > 1) {
    throw NoAnswer{};
  }
  if (call.size() == 0 || call.missing(0)) {
    return Value{1.0 +
                 (of_row ? call.cell().cell.row : call.cell().cell.column)};
  }
  const Value value = call.value(0);
  const auto *reference = std::get_if<Reference>(&value.content);
  if (reference == nullptr || reference->areas.size() != 1) {
    throw NoAnswer{};
  }
  const TablePosition &from = reference->areas.front().range.from();
  return Value{1.0 + (of_row ? from.row : from.column)};
}

/// `ROWS` and `COLUMNS`: the size of a range or an array. A whole column is
/// as tall as the grid of the application, which the two state apart.
template <bool of_rows> Value size(const Call &call) {
  expect_arguments(call, 1, 1);
  const Value value = call.value(0);
  if (const auto *reference = std::get_if<Reference>(&value.content)) {
    if (reference->areas.size() != 1) {
      throw NoAnswer{};
    }
    const Area &area = reference->areas.front();
    if (of_rows ? area.whole_columns : area.whole_rows) {
      throw NoAnswer{};
    }
    return Value{1.0 +
                 (of_rows ? area.range.to().row - area.range.from().row
                          : area.range.to().column - area.range.from().column)};
  }
  const Cells cells(call, value);
  return Value{static_cast<double>(of_rows ? cells.rows : cells.columns)};
}

constexpr std::array entries{
    FunctionEntry{"AVERAGEIF", sum_if<true>},
    FunctionEntry{"AVERAGEIFS", sum_ifs<true>},
    FunctionEntry{"COLUMN", position<false>},
    FunctionEntry{"COLUMNS", size<false>},
    FunctionEntry{"COUNTIF", count_if<2>},
    FunctionEntry{"COUNTIFS", count_if<254>},
    FunctionEntry{"HLOOKUP", lookup<false>},
    FunctionEntry{"INDEX", index},
    FunctionEntry{"INDIRECT", indirect},
    FunctionEntry{"MATCH", match},
    FunctionEntry{"OFFSET", offset},
    FunctionEntry{"ROW", position<true>},
    FunctionEntry{"ROWS", size<true>},
    FunctionEntry{"SUMIF", sum_if<false>},
    FunctionEntry{"SUMIFS", sum_ifs<false>},
    FunctionEntry{"VLOOKUP", lookup<true>},
};

} // namespace

} // namespace odr::internal::formula

namespace odr::internal {

std::span<const formula::FunctionEntry> formula::lookup_functions() {
  return entries;
}

} // namespace odr::internal
