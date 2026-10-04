#include <odr/internal/formula/formula_function.hpp>
#include <odr/internal/formula/formula_text.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <set>
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

/// Whether @p text holds a character a criterion or a lookup reads as more
/// than itself: a wildcard in Excel and where an ods turns them on, the
/// characters of a regular expression where an ods turns those on.
bool is_pattern(const Call &call, const std::string_view text) {
  const bool wildcards = !is_libreoffice(call) || call.settings().wildcards;
  const bool expressions =
      is_libreoffice(call) && call.settings().regular_expressions;
  for (const char c : text) {
    if (wildcards && (c == '*' || c == '?' || c == '~')) {
      return true;
    }
    if (expressions &&
        std::string_view(".^$*+?()[]{}|\\").find(c) != std::string_view::npos) {
      return true;
    }
  }
  return false;
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
      columns = m_area->range.to().column - m_area->range.from().column + 1;
      rows = m_area->range.to().row - m_area->range.from().row + 1;
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

  [[nodiscard]] const std::optional<Area> &area() const { return m_area; }

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
  std::optional<std::string> m_text;

  static bool is_truth_text(const std::string_view text) {
    return equals_without_case(text, "TRUE") ||
           equals_without_case(text, "FALSE");
  }
  static bool equals_without_case(const std::string_view a,
                                  const std::string_view b) {
    return a.size() == b.size() && same_text(a, b);
  }

  /// How @p value orders against the criterion, nothing where the two do not
  /// compare: a number and a text.
  [[nodiscard]] std::optional<std::strong_ordering>
  order_with(const Value &value) const {
    if (m_number.has_value()) {
      if (const auto *number = std::get_if<double>(&value.content)) {
        if (approximately_equal(*number, *m_number)) {
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
          approximately_equal(std::get<double>(*read), *m_number)) {
        return std::strong_ordering::equal;
      }
      return std::nullopt;
    }
    const auto *text = std::get_if<std::string>(&value.content);
    if (text == nullptr) {
      return std::nullopt;
    }
    if (m_relation == Relation::equal || m_relation == Relation::not_equal) {
      return same_text(*text, *m_text) ? std::strong_ordering::equal
                                       : std::strong_ordering::less;
    }
    return order_of_texts(*text, *m_text, false);
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
      if (is_libreoffice(call)) {
        throw NoAnswer{};
      }
      throw ErrorResult{ErrorType::value};
    }
  }
  return result;
}

/// The most positions the conditions of one call test.
constexpr std::size_t cell_limit = 1 << 22;

/// Calls @p visit with the position of every cell that meets all
/// @p conditions.
template <typename Visit>
void each_match(const std::span<const Condition> conditions,
                const Visit &visit) {
  const Cells &shape = conditions.front().cells;
  if (std::size_t{shape.columns} * shape.rows > cell_limit) {
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
  std::set<ErrorType> errors;
  each_match(
      conditions, [&](const std::uint32_t column, const std::uint32_t row) {
        const Value value = as_dialect(call, values.at(column, row));
        if (const auto *number = std::get_if<double>(&value.content)) {
          numbers.push_back(*number);
        } else if (const auto *error = std::get_if<ErrorType>(&value.content)) {
          errors.insert(*error);
        }
      });
  if (errors.size() > 1) {
    throw NoAnswer{};
  }
  if (!errors.empty()) {
    throw ErrorResult{*errors.begin()};
  }
  if (average && numbers.empty()) {
    return Value{ErrorType::division};
  }
  const Value sum = sum_of(call, numbers);
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
           approximately_equal(value.get<double>(), *number);
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

/// The position of @p wanted along a line of @p count cells, read by
/// @p cell_at. An exact lookup takes the first match. An approximate one
/// takes the last number not past @p wanted, where every cell is a number in
/// ascending order and the one found is not repeated; anything else is for
/// the binary search of the application, which the two do apart.
template <typename CellAt>
std::optional<std::uint32_t>
position_of(const Call &call, const Value &wanted, const std::uint32_t count,
            const bool exact, const CellAt &cell_at) {
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
    if (*cell <= *number || approximately_equal(*cell, *number)) {
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
  const Number stated = call.number(call.scalar(index));
  if (const auto *error = std::get_if<ErrorType>(&stated)) {
    throw ErrorResult{*error};
  }
  return std::get<double>(stated) == 0;
}

/// `VLOOKUP` and `HLOOKUP`: the value in line @p index of the table, of the
/// first column or row matching.
template <bool vertical> Value lookup(const Call &call) {
  expect_arguments(call, 3, 4);
  const Value wanted = wanted_of(call, 0);
  const Cells table(call, call.value(1));
  const Number index_read = call.number(call.scalar(2));
  if (const auto *error = std::get_if<ErrorType>(&index_read)) {
    return Value{*error};
  }
  const double index = std::trunc(std::get<double>(index_read));
  const std::uint32_t lines = vertical ? table.columns : table.rows;
  if (index < 1) {
    return refused(call, ErrorType::value);
  }
  if (index > lines) {
    return refused(call, ErrorType::reference);
  }
  const std::uint32_t count = vertical ? table.rows : table.columns;
  const std::optional<std::uint32_t> found =
      position_of(call, wanted, count, exact_of(call, 3, false),
                  [&](const std::uint32_t i) {
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
  double type = 1;
  if (call.size() > 2) {
    const Number stated = call.number(call.scalar(2));
    if (const auto *error = std::get_if<ErrorType>(&stated)) {
      return Value{*error};
    }
    type = std::get<double>(stated);
  }
  if (type < 0) {
    throw NoAnswer{}; // descending order
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
    const Number number = call.number(call.scalar(i));
    if (const auto *error = std::get_if<ErrorType>(&number)) {
      throw ErrorResult{*error};
    }
    return std::trunc(std::get<double>(number));
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
    return Value{ErrorType::reference};
  }
  return cells.reference_at(static_cast<std::uint32_t>(column) - 1,
                            static_cast<std::uint32_t>(row) - 1);
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
    FunctionEntry{"MATCH", match},
    FunctionEntry{"ROW", position<true>},
    FunctionEntry{"ROWS", size<true>},
    FunctionEntry{"SUMIF", sum_if<false>},
    FunctionEntry{"SUMIFS", sum_ifs<false>},
    FunctionEntry{"VLOOKUP", lookup<true>},
};

} // namespace

std::span<const FunctionEntry> lookup_functions() { return entries; }

} // namespace odr::internal::formula
