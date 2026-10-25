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
Value as_dialect(const Call &call, const Value &value) noexcept {
  if (const auto *boolean = std::get_if<bool>(&value.content);
      boolean != nullptr && is_libreoffice(call)) {
    return Value{*boolean ? 1.0 : 0.0};
  }
  return value;
}

/// Whether two texts are one without case, as a criterion and a lookup
/// compare them in both applications, whatever the settings.
Result<bool> same_text(const std::string_view a,
                       const std::string_view b) noexcept {
  return equal_texts(a, b, false);
}

/// A rectangle of cells by position, read row by row.
class Cells final {
public:
  /// The cells of @p value: a range, an array, or one value. Stops with the
  /// error where it is one.
  [[nodiscard]] static Result<Cells> of(const Call &call,
                                        const Value &value) noexcept {
    Cells result;
    result.m_call = &call;
    if (const auto *reference = std::get_if<Reference>(&value.content)) {
      if (reference->areas.size() != 1) {
        return no_answer();
      }
      const Area &area = reference->areas.front();
      result.m_area = area;
      result.whole = area.whole_columns || area.whole_rows;
      const auto width =
          std::uint64_t{area.range.to().column} - area.range.from().column + 1;
      const auto height =
          std::uint64_t{area.range.to().row} - area.range.from().row + 1;
      if (width > std::numeric_limits<std::uint32_t>::max() ||
          height > std::numeric_limits<std::uint32_t>::max()) {
        return no_answer();
      }
      result.columns = static_cast<std::uint32_t>(width);
      result.rows = static_cast<std::uint32_t>(height);
    } else if (const auto *matrix = std::get_if<Matrix>(&value.content)) {
      result.m_matrix = *matrix;
      result.columns = matrix->columns;
      result.rows = matrix->rows;
    } else if (const auto *error = std::get_if<ErrorType>(&value.content)) {
      return error_result(*error);
    } else {
      result.m_matrix = Matrix{1, 1, {value}};
      result.columns = 1;
      result.rows = 1;
    }
    return result;
  }

  std::uint32_t columns{0};
  std::uint32_t rows{0};
  /// Whether the cells are a whole column or row, which the extent of its
  /// sheet cuts and the grid of an application does not.
  bool whole{false};

  [[nodiscard]] Result<Value> at(const std::uint32_t column,
                                 const std::uint32_t row) const noexcept {
    if (m_area.has_value()) {
      return m_call->cell_value(
          SheetPosition(m_area->sheet, m_area->range.from().column + column,
                        m_area->range.from().row + row));
    }
    return m_matrix.cells[std::size_t{row} * m_matrix.columns + column];
  }

  /// The reference to the cell at (@p column, @p row), or its value where
  /// the cells are an array.
  [[nodiscard]] Result<Value>
  reference_at(const std::uint32_t column,
               const std::uint32_t row) const noexcept {
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

  Cells() = default;
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
  /// The criterion @p stated spells. Stops with the error where it is one.
  [[nodiscard]] static Result<Criterion> of(const Call &call,
                                            const Value &stated) noexcept {
    Criterion result;
    result.m_call = &call;
    const Value value = as_dialect(call, stated);
    if (const auto *error = std::get_if<ErrorType>(&value.content)) {
      return error_result(*error);
    }
    if (const auto *number = std::get_if<double>(&value.content)) {
      result.m_number = *number;
      result.m_exact_number = true;
      return result;
    }
    const auto *text = std::get_if<std::string>(&value.content);
    if (text == nullptr) {
      return no_answer(); // an empty cell, or a boolean in Excel
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
        result.m_relation = relation;
        result.m_stated_relation = true;
        rest.remove_prefix(spelling.size());
        break;
      }
    }
    if (rest.empty()) {
      if (result.m_relation != Relation::equal &&
          result.m_relation != Relation::not_equal) {
        return no_answer();
      }
      result.m_blank = true;
      return result;
    }
    if (const std::optional<Number> number = number_of_text(rest);
        number.has_value()) {
      if (const auto *read = std::get_if<double>(&*number)) {
        result.m_number = *read;
        return result;
      }
    }
    const Result<bool> truth = is_truth_text(rest);
    if (!truth.has_value()) {
      return std::unexpected(truth.error());
    }
    if (*truth || is_pattern(call, rest) || !call.settings().whole_cell) {
      return no_answer();
    }
    result.m_text = std::string(rest);
    return result;
  }

  [[nodiscard]] Result<bool> matches(const Value &stated) const noexcept {
    const Value value = as_dialect(*m_call, stated);
    if (m_blank) {
      // `""` matches an empty cell and an empty text, `"="` an empty cell
      const bool empty = value.holds<Empty>();
      const auto *text = std::get_if<std::string>(&value.content);
      const bool empty_text = text != nullptr && text->empty();
      if (m_relation == Relation::not_equal) {
        return !empty;
      }
      return empty || (empty_text && !m_stated_relation);
    }
    if (value.holds<ErrorType>()) {
      if (m_relation == Relation::not_equal) {
        return no_answer();
      }
      return false;
    }
    const Result<std::partial_ordering> order = order_with(value);
    if (!order.has_value()) {
      return std::unexpected(order.error());
    }
    if (*order == std::partial_ordering::unordered) {
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

  Criterion() = default;

  static Result<bool> is_truth_text(const std::string_view text) noexcept {
    const Result<bool> is_true = same_text(text, "TRUE");
    if (!is_true.has_value() || *is_true) {
      return is_true;
    }
    return same_text(text, "FALSE");
  }

  /// How @p value orders against the criterion, unordered where the two do
  /// not compare: a number and a text.
  [[nodiscard]] Result<std::partial_ordering>
  order_with(const Value &value) const noexcept {
    if (m_number.has_value()) {
      if (const auto *number = std::get_if<double>(&value.content)) {
        const Result<bool> same =
            same_number(m_call->settings().dialect, *number, *m_number);
        if (!same.has_value()) {
          return std::unexpected(same.error());
        }
        if (*same) {
          return std::partial_ordering::equivalent;
        }
        return *number < *m_number ? std::partial_ordering::less
                                   : std::partial_ordering::greater;
      }
      const auto *text = std::get_if<std::string>(&value.content);
      if (text == nullptr || m_exact_number) {
        if (text != nullptr && !is_libreoffice(*m_call)) {
          // whether Excel reads a text as the number it spells here is not
          // documented
          const std::optional<Number> read = number_of_text(*text);
          if (read.has_value() && std::holds_alternative<double>(*read)) {
            return no_answer();
          }
        }
        return std::partial_ordering::unordered;
      }
      // a text spelling the number is the number for `=` and `<>`
      const std::optional<Number> read = number_of_text(*text);
      if (!read.has_value() && std::ranges::any_of(*text, [](const char c) {
            return c >= '0' && c <= '9';
          })) {
        return no_answer();
      }
      if (m_relation != Relation::equal && m_relation != Relation::not_equal) {
        return std::partial_ordering::unordered;
      }
      const double *spelled =
          read.has_value() ? std::get_if<double>(&*read) : nullptr;
      if (spelled == nullptr) {
        return std::partial_ordering::unordered;
      }
      const Result<bool> same =
          same_number(m_call->settings().dialect, *spelled, *m_number);
      if (!same.has_value()) {
        return std::unexpected(same.error());
      }
      if (*same) {
        return std::partial_ordering::equivalent;
      }
      return std::partial_ordering::unordered;
    }
    const auto *text = std::get_if<std::string>(&value.content);
    if (text == nullptr) {
      return std::partial_ordering::unordered;
    }
    if (m_relation == Relation::equal || m_relation == Relation::not_equal) {
      return same_text(*text, m_text).transform([](const bool same) {
        return same ? std::partial_ordering::equivalent
                    : std::partial_ordering::less;
      });
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

/// The condition of the range argument @p cells and the criterion argument
/// @p criterion, read in this order.
Result<Condition> condition_of(const Call &call, const std::size_t cells,
                               const std::size_t criterion) noexcept {
  Result<Cells> tested = call.value(cells).and_then(
      [&](const Value &value) { return Cells::of(call, value); });
  if (!tested.has_value()) {
    return std::unexpected(tested.error());
  }
  Result<Criterion> stated = call.scalar(criterion).and_then(
      [&](const Value &value) { return Criterion::of(call, value); });
  if (!stated.has_value()) {
    return std::unexpected(stated.error());
  }
  return Condition{std::move(*tested), std::move(*stated)};
}

Result<std::vector<Condition>> conditions(const Call &call,
                                          const std::size_t first) noexcept {
  if (call.size() <= first || (call.size() - first) % 2 != 0) {
    return no_answer();
  }
  std::vector<Condition> result;
  for (std::size_t i = first; i < call.size(); i += 2) {
    if (call.missing(i) || call.missing(i + 1)) {
      return no_answer();
    }
    Result<Condition> condition = condition_of(call, i, i + 1);
    if (!condition.has_value()) {
      return std::unexpected(condition.error());
    }
    result.push_back(std::move(*condition));
    if (result.back().cells.columns != result.front().cells.columns ||
        result.back().cells.rows != result.front().cells.rows) {
      // whole columns of two sheets differ by the extents, not the grid
      if (is_libreoffice(call) || result.back().cells.whole ||
          result.front().cells.whole) {
        return no_answer();
      }
      return error_result(ErrorType::value);
    }
  }
  return result;
}

/// The most positions the conditions of one call test.
constexpr std::size_t cell_limit = 1 << 22;

/// Visits cells matching all @p conditions. Refuses unbounded ranges when
/// empty cells beyond the sheet extent would also match. A stop of @p visit
/// ends the walk and is its result.
template <typename Visit>
Result<void> each_match(const std::span<const Condition> conditions,
                        const Visit &visit) noexcept {
  const Cells &shape = conditions.front().cells;
  if (std::uint64_t{shape.columns} * shape.rows > cell_limit) {
    return no_answer();
  }
  if (std::ranges::any_of(conditions,
                          [](const Condition &c) { return c.cells.whole; })) {
    bool all = true;
    for (const Condition &condition : conditions) {
      const Result<bool> matches = condition.criterion.matches(Value{Empty{}});
      if (!matches.has_value()) {
        return std::unexpected(matches.error());
      }
      if (!*matches) {
        all = false;
        break;
      }
    }
    if (all) {
      return no_answer();
    }
  }
  for (std::uint32_t row = 0; row < shape.rows; ++row) {
    for (std::uint32_t column = 0; column < shape.columns; ++column) {
      bool all = true;
      for (const Condition &condition : conditions) {
        const Result<bool> matches =
            condition.cells.at(column, row).and_then([&](const Value &cell) {
              return condition.criterion.matches(cell);
            });
        if (!matches.has_value()) {
          return std::unexpected(matches.error());
        }
        if (!*matches) {
          all = false;
          break;
        }
      }
      if (all) {
        if (const Result<void> visited = visit(column, row);
            !visited.has_value()) {
          return visited;
        }
      }
    }
  }
  return {};
}

/// `COUNTIF` and `COUNTIFS`, which take up to @p most arguments.
template <std::size_t most> Result<Value> count_if(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 2, most);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::vector<Condition>> tested = conditions(call, 0);
  if (!tested.has_value()) {
    return std::unexpected(tested.error());
  }
  double result = 0;
  const auto visit = [&](std::uint32_t, std::uint32_t) -> Result<void> {
    ++result;
    return {};
  };
  if (const Result<void> walked = each_match(*tested, visit);
      !walked.has_value()) {
    return std::unexpected(walked.error());
  }
  return Value{result};
}

/// The sum of @p values at the positions that meet @p conditions, or their
/// average where @p average. A text and an empty cell add nothing, an error
/// is the result.
Result<Value> matched(const Call &call,
                      const std::span<const Condition> conditions,
                      const Cells &values, const bool average) noexcept {
  std::vector<double> numbers;
  Errors errors;
  const auto visit = [&](const std::uint32_t column,
                         const std::uint32_t row) -> Result<void> {
    const Result<Value> cell = values.at(column, row);
    if (!cell.has_value()) {
      return std::unexpected(cell.error());
    }
    const Value value = as_dialect(call, *cell);
    if (const auto *number = std::get_if<double>(&value.content)) {
      numbers.push_back(*number);
    } else if (const auto *error = std::get_if<ErrorType>(&value.content)) {
      errors.add(*error);
    }
    return {};
  };
  if (const Result<void> walked = each_match(conditions, visit);
      !walked.has_value()) {
    return std::unexpected(walked.error());
  }
  const Result<std::optional<ErrorType>> error = errors.first();
  if (!error.has_value()) {
    return std::unexpected(error.error());
  }
  if (error->has_value()) {
    return error_result(**error);
  }
  if (average && numbers.empty()) {
    return Value{ErrorType::division};
  }
  Result<Value> sum = sum_of(call, numbers);
  if (!average || !sum.has_value() || !sum->holds<double>()) {
    return sum;
  }
  return Value{sum->get<double>() / static_cast<double>(numbers.size())};
}

/// The range a `SUMIF` adds: the one it names, of the size of the range it
/// tests, from its first cell on, as both applications read it.
Result<Cells> sum_range(const Call &call, const Cells &tested,
                        const std::size_t index) noexcept {
  if (index >= call.size()) {
    return tested;
  }
  if (call.missing(index)) {
    return no_answer();
  }
  Result<Cells> stated = call.value(index).and_then(
      [&](const Value &value) { return Cells::of(call, value); });
  if (!stated.has_value()) {
    return stated;
  }
  if (stated->columns != tested.columns || stated->rows != tested.rows) {
    return no_answer();
  }
  return stated;
}

template <bool average> Result<Value> sum_if(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 2, 3);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  Result<Condition> condition = condition_of(call, 0, 1);
  if (!condition.has_value()) {
    return std::unexpected(condition.error());
  }
  const std::vector<Condition> tested{std::move(*condition)};
  const Result<Cells> values = sum_range(call, tested.front().cells, 2);
  if (!values.has_value()) {
    return std::unexpected(values.error());
  }
  return matched(call, tested, *values, average);
}

template <bool average> Result<Value> sum_ifs(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 3, 255);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<std::vector<Condition>> tested = conditions(call, 1);
  if (!tested.has_value()) {
    return std::unexpected(tested.error());
  }
  const Result<Cells> values = call.value(0).and_then(
      [&](const Value &value) { return Cells::of(call, value); });
  if (!values.has_value()) {
    return std::unexpected(values.error());
  }
  if (values->columns != tested->front().cells.columns ||
      values->rows != tested->front().cells.rows) {
    if (values->whole || tested->front().cells.whole) {
      return no_answer();
    }
    return refused(call, ErrorType::value);
  }
  return matched(call, *tested, *values, average);
}

/// Whether a cell is the value a lookup asks for: a number the same number,
/// a text the same text without case. A pattern has no answer.
Result<bool> is_lookup_match(const Call &call, const Value &wanted,
                             const Value &stated) noexcept {
  const Value value = as_dialect(call, stated);
  if (const auto *number = std::get_if<double>(&wanted.content)) {
    const auto *cell = std::get_if<double>(&value.content);
    if (cell == nullptr) {
      return false;
    }
    return same_number(call.settings().dialect, *cell, *number);
  }
  if (const auto *text = std::get_if<std::string>(&wanted.content)) {
    const auto *cell = std::get_if<std::string>(&value.content);
    if (cell == nullptr) {
      return false;
    }
    return same_text(*cell, *text);
  }
  if (const auto *boolean = std::get_if<bool>(&wanted.content)) {
    const auto *cell = std::get_if<bool>(&value.content);
    return cell != nullptr && *cell == *boolean;
  }
  return no_answer();
}

/// The value a lookup asks for, as a scalar.
Result<Value> wanted_of(const Call &call, const std::size_t index) noexcept {
  const Result<Value> scalar = call.scalar(index);
  if (!scalar.has_value()) {
    return scalar;
  }
  Value wanted = as_dialect(call, *scalar);
  if (const auto *error = std::get_if<ErrorType>(&wanted.content)) {
    return error_result(*error);
  }
  if (wanted.holds<Empty>()) {
    return no_answer();
  }
  if (const auto *text = std::get_if<std::string>(&wanted.content);
      text != nullptr && is_pattern(call, *text)) {
    return no_answer();
  }
  return wanted;
}

/// Finds the first exact match, or the last number <= @p wanted. Approximate
/// lookup requires ascending numbers and a unique match; other cases depend
/// on application-specific binary search.
template <typename CellAt>
Result<std::optional<std::uint32_t>>
position_of(const Call &call, const Value &wanted, const std::uint32_t count,
            const bool exact, const CellAt &cell_at) noexcept {
  if (count > cell_limit) {
    return no_answer();
  }
  if (exact) {
    for (std::uint32_t i = 0; i < count; ++i) {
      const Result<bool> matches = cell_at(i).and_then([&](const Value &cell) {
        return is_lookup_match(call, wanted, cell);
      });
      if (!matches.has_value()) {
        return std::unexpected(matches.error());
      }
      if (*matches) {
        return i;
      }
    }
    return std::nullopt;
  }
  const auto *number = std::get_if<double>(&wanted.content);
  if (number == nullptr) {
    return no_answer();
  }
  std::optional<std::uint32_t> found;
  std::optional<double> previous;
  for (std::uint32_t i = 0; i < count; ++i) {
    const Result<Value> stated = cell_at(i);
    if (!stated.has_value()) {
      return std::unexpected(stated.error());
    }
    const Value value = as_dialect(call, *stated);
    const auto *cell = std::get_if<double>(&value.content);
    if (cell == nullptr || (previous.has_value() && *cell < *previous)) {
      return no_answer();
    }
    if (previous.has_value() && *cell == *previous &&
        found == static_cast<std::uint32_t>(i - 1)) {
      return no_answer();
    }
    if (*cell <= *number) {
      found = i;
    } else {
      const Result<bool> same =
          same_number(call.settings().dialect, *cell, *number);
      if (!same.has_value()) {
        return std::unexpected(same.error());
      }
      if (*same) {
        found = i;
      }
    }
    previous = *cell;
  }
  return found;
}

/// Whether a lookup is exact, as its last argument states: 0 or false.
Result<bool> exact_of(const Call &call, const std::size_t index,
                      const bool unstated) noexcept {
  if (index >= call.size()) {
    return unstated;
  }
  return number_argument(call, index).transform([](const double x) {
    return x == 0;
  });
}

/// `VLOOKUP` and `HLOOKUP`: the value in line @p index of the table, of the
/// first column or row matching.
template <bool vertical> Result<Value> lookup(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 3, 4);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<Value> wanted = wanted_of(call, 0);
  if (!wanted.has_value()) {
    return wanted;
  }
  const Result<Cells> table = call.value(1).and_then(
      [&](const Value &value) { return Cells::of(call, value); });
  if (!table.has_value()) {
    return std::unexpected(table.error());
  }
  const Result<double> stated_index = number_argument(call, 2);
  if (!stated_index.has_value()) {
    return std::unexpected(stated_index.error());
  }
  const double index = std::trunc(*stated_index);
  const std::uint32_t lines = vertical ? table->columns : table->rows;
  if (index < 1) {
    return refused(call, ErrorType::value);
  }
  if (index > lines) {
    return refused(call, ErrorType::reference);
  }
  const std::uint32_t count = vertical ? table->rows : table->columns;
  const Result<bool> exact = exact_of(call, 3, false);
  if (!exact.has_value()) {
    return std::unexpected(exact.error());
  }
  // an approximate lookup's search meets the empty cells past the extent
  if (!*exact && table->whole) {
    return no_answer();
  }
  const Result<std::optional<std::uint32_t>> found =
      position_of(call, *wanted, count, *exact, [&](const std::uint32_t i) {
        return vertical ? table->at(0, i) : table->at(i, 0);
      });
  if (!found.has_value()) {
    return std::unexpected(found.error());
  }
  if (!found->has_value()) {
    return Value{ErrorType::not_available};
  }
  const auto line = static_cast<std::uint32_t>(index) - 1;
  return vertical ? table->at(line, **found) : table->at(**found, line);
}

/// `MATCH`: the position of a value in a row or a column, counted from 1.
Result<Value> match(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 2, 3);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<Value> wanted = wanted_of(call, 0);
  if (!wanted.has_value()) {
    return wanted;
  }
  const Result<Cells> line = call.value(1).and_then(
      [&](const Value &value) { return Cells::of(call, value); });
  if (!line.has_value()) {
    return std::unexpected(line.error());
  }
  if (line->columns != 1 && line->rows != 1) {
    return Value{ErrorType::not_available};
  }
  double type = 1;
  if (call.size() > 2) {
    const Result<double> stated = number_argument(call, 2);
    if (!stated.has_value()) {
      return std::unexpected(stated.error());
    }
    type = *stated;
  }
  if (type < 0) {
    return no_answer(); // descending order
  }
  if (type != 0 && line->whole) {
    return no_answer();
  }
  const bool vertical = line->columns == 1;
  const Result<std::optional<std::uint32_t>> found =
      position_of(call, *wanted, vertical ? line->rows : line->columns,
                  type == 0, [&](const std::uint32_t i) {
                    return vertical ? line->at(0, i) : line->at(i, 0);
                  });
  if (!found.has_value()) {
    return std::unexpected(found.error());
  }
  if (!found->has_value()) {
    return Value{ErrorType::not_available};
  }
  return Value{static_cast<double>(**found + 1)};
}

/// Argument @p index as a whole number, toward 0.
Result<double> truncated_argument(const Call &call,
                                  const std::size_t index) noexcept {
  return number_argument(call, index).transform([](const double x) {
    return std::trunc(x);
  });
}

/// `INDEX`: the cell at a row and a column of a range, counted from 1. A
/// single row or column takes one index.
Result<Value> index(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 2, 3);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<Cells> cells = call.value(0).and_then(
      [&](const Value &value) { return Cells::of(call, value); });
  if (!cells.has_value()) {
    return std::unexpected(cells.error());
  }
  const Result<double> stated_row = truncated_argument(call, 1);
  if (!stated_row.has_value()) {
    return std::unexpected(stated_row.error());
  }
  double row = *stated_row;
  double column = 1;
  if (call.size() > 2) {
    const Result<double> stated_column = truncated_argument(call, 2);
    if (!stated_column.has_value()) {
      return std::unexpected(stated_column.error());
    }
    column = *stated_column;
  }
  if (call.size() == 2 && cells->rows == 1 && cells->columns > 1) {
    column = row;
    row = 1;
  } else if (call.size() == 2 && cells->columns > 1) {
    return no_answer(); // a whole row of a table
  }
  if (row == 0 || column == 0) {
    return no_answer(); // a whole row or column
  }
  if (row < 0 || column < 0) {
    return refused(call, ErrorType::value);
  }
  if (row > cells->rows || column > cells->columns) {
    if (cells->whole) {
      return no_answer(); // past the extent, not the grid
    }
    return Value{ErrorType::reference};
  }
  return cells->reference_at(static_cast<std::uint32_t>(column) - 1,
                             static_cast<std::uint32_t>(row) - 1);
}

/// The grid of Excel. LibreOffice refuses a reference past its own grid with
/// a code a file cannot hold, so the size of its grid decides nothing here.
constexpr double grid_columns = 16384;
constexpr double grid_rows = 1048576;

/// `OFFSET`: a reference moved by rows and columns, and sized anew where a
/// height or a width is given.
Result<Value> offset(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 3, 5);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<Value> value = call.value(0);
  if (!value.has_value()) {
    return value;
  }
  if (const auto *error = std::get_if<ErrorType>(&value->content)) {
    return Value{*error};
  }
  const auto *reference = std::get_if<Reference>(&value->content);
  if (reference == nullptr || reference->areas.size() != 1) {
    return no_answer();
  }
  const Area &area = reference->areas.front();
  // whether a whole row or column is moved or sized
  const auto changed = [&](const std::size_t sized,
                           const std::size_t moved) -> Result<bool> {
    if (call.size() > sized) {
      return true;
    }
    return number_argument(call, moved).transform([](const double x) {
      return x != 0;
    });
  };
  if (area.whole_columns) {
    const Result<bool> refuses = changed(3, 1);
    if (!refuses.has_value()) {
      return std::unexpected(refuses.error());
    }
    if (*refuses) {
      return no_answer();
    }
  }
  if (area.whole_rows) {
    const Result<bool> refuses = changed(4, 2);
    if (!refuses.has_value()) {
      return std::unexpected(refuses.error());
    }
    if (*refuses) {
      return no_answer();
    }
  }
  const TablePosition &from = area.range.from();
  const TablePosition &to = area.range.to();
  const Result<double> rows = truncated_argument(call, 1);
  if (!rows.has_value()) {
    return std::unexpected(rows.error());
  }
  const Result<double> columns = truncated_argument(call, 2);
  if (!columns.has_value()) {
    return std::unexpected(columns.error());
  }
  double height = to.row - from.row + 1.0;
  if (call.size() > 3) {
    const Result<double> stated = truncated_argument(call, 3);
    if (!stated.has_value()) {
      return std::unexpected(stated.error());
    }
    height = *stated;
  }
  double width = to.column - from.column + 1.0;
  if (call.size() > 4) {
    const Result<double> stated = truncated_argument(call, 4);
    if (!stated.has_value()) {
      return std::unexpected(stated.error());
    }
    width = *stated;
  }
  const double top = from.row + *rows;
  const double left = from.column + *columns;
  if (height < 1 || width < 1) {
    return no_answer(); // Excel does not document it, LibreOffice refuses it
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
                                    const double limit) noexcept {
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
                                   const TablePosition &at) noexcept {
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
                                 const bool libreoffice) noexcept {
  const auto is_reference = [](const std::optional<Node> &node) {
    return node.has_value() &&
           (node->holds<CellReference>() || node->holds<RangeReference>() ||
            node->holds<NameReference>() ||
            (node->holds<BinaryOperation>() &&
             std::get_if<BinaryOperation>(&node->content)->op ==
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
bool spaced(const std::string_view text) noexcept {
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
Result<Value> indirect(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 1, 2);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<Value> argument = call.scalar(0);
  if (!argument.has_value()) {
    return argument;
  }
  if (const auto *error = std::get_if<ErrorType>(&argument->content)) {
    return Value{*error};
  }
  const auto *text = std::get_if<std::string>(&argument->content);
  if (text == nullptr) {
    return Value{ErrorType::reference};
  }
  if (spaced(*text)) {
    if (is_libreoffice(call)) {
      return Value{ErrorType::reference};
    }
    return no_answer(); // whether Excel trims is not documented
  }
  bool a1 = true;
  if (call.size() > 1) {
    const Result<Value> flag = call.scalar(1);
    if (!flag.has_value()) {
      return flag;
    }
    if (const auto *error = std::get_if<ErrorType>(&flag->content)) {
      return Value{*error};
    }
    if (const auto *boolean = std::get_if<bool>(&flag->content)) {
      a1 = *boolean;
    } else if (const auto *number = std::get_if<double>(&flag->content)) {
      a1 = *number != 0;
    } else {
      return no_answer();
    }
  }
  if (a1) {
    const std::optional<Node> node = a1_reference(*text, is_libreoffice(call));
    if (!node.has_value()) {
      return Value{ErrorType::reference};
    }
    return call.reference(*node);
  }
  const std::optional<Node> node = r1c1_reference(*text, call.cell().cell);
  if (!node.has_value()) {
    return no_answer(); // a whole row or column, which this does not read
  }
  return call.reference(*node);
}

/// `ROW` and `COLUMN`: of the first cell of a reference, or of the formula's
/// own cell.
template <bool of_row> Result<Value> position(const Call &call) noexcept {
  if (call.size() > 1) {
    return no_answer();
  }
  if (call.size() == 0 || call.missing(0)) {
    return Value{1.0 +
                 (of_row ? call.cell().cell.row : call.cell().cell.column)};
  }
  const Result<Value> value = call.value(0);
  if (!value.has_value()) {
    return value;
  }
  const auto *reference = std::get_if<Reference>(&value->content);
  if (reference == nullptr || reference->areas.size() != 1) {
    return no_answer();
  }
  const TablePosition &from = reference->areas.front().range.from();
  return Value{1.0 + (of_row ? from.row : from.column)};
}

/// `ROWS` and `COLUMNS`: the size of a range or an array. A whole column is
/// as tall as the grid of the application, which the two state apart.
template <bool of_rows> Result<Value> size(const Call &call) noexcept {
  if (const Result<void> expected = expect_arguments(call, 1, 1);
      !expected.has_value()) {
    return std::unexpected(expected.error());
  }
  const Result<Value> value = call.value(0);
  if (!value.has_value()) {
    return value;
  }
  if (const auto *reference = std::get_if<Reference>(&value->content)) {
    if (reference->areas.size() != 1) {
      return no_answer();
    }
    const Area &area = reference->areas.front();
    if (of_rows ? area.whole_columns : area.whole_rows) {
      return no_answer();
    }
    return Value{1.0 +
                 (of_rows ? area.range.to().row - area.range.from().row
                          : area.range.to().column - area.range.from().column)};
  }
  return Cells::of(call, *value).transform([](const Cells &cells) {
    return Value{static_cast<double>(of_rows ? cells.rows : cells.columns)};
  });
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

std::span<const formula::FunctionEntry> formula::lookup_functions() noexcept {
  return entries;
}

} // namespace odr::internal
