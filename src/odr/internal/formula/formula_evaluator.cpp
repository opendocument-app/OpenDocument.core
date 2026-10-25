#include <odr/internal/formula/formula_evaluator.hpp>

#include <odr/internal/formula/formula_function.hpp>
#include <odr/internal/formula/formula_text.hpp>
#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cmath>
#include <compare>
#include <functional>
#include <type_traits>
#include <utility>

namespace odr::internal::formula {

namespace str = util::string;

namespace {

/// Checks array size before narrowing or allocating, including on 32-bit hosts.
Result<std::size_t> array_size(const std::uint64_t columns,
                               const std::uint64_t rows) {
  constexpr std::uint64_t limit = 1 << 20;
  if (columns > limit || rows > limit || columns * rows > limit) {
    return no_answer();
  }
  return static_cast<std::size_t>(columns * rows);
}

/// The last index of an axis that states @p count positions.
std::uint32_t last_of(const std::uint32_t count) {
  return count == 0 ? 0 : count - 1;
}

} // namespace

/// Evaluates the nodes of one formula. A reference stays one until an
/// operator or a function reads it.
class Evaluator final {
public:
  Evaluator(const CellSource *source, const Settings *settings,
            const SheetPosition &cell, const Node *root)
      : m_source{source}, m_settings{settings}, m_cell{cell}, m_root{root} {}

  [[nodiscard]] const Settings &settings() const { return *m_settings; }
  [[nodiscard]] const SheetPosition &cell() const { return m_cell; }

  [[nodiscard]] Result<Value> value(const Node &node) {
    return std::visit(
        [&](const auto &content) -> Result<Value> {
          return value_of(content, node);
        },
        node.content);
  }

  /// @p value as one value: a range gives the cell the formula's row or
  /// column crosses, an array its first element.
  [[nodiscard]] Result<Value> scalar(Value value) const {
    if (const auto *reference = std::get_if<Reference>(&value.content)) {
      return intersection(*reference);
    }
    if (const auto *matrix = std::get_if<Matrix>(&value.content)) {
      if (matrix->cells.empty()) {
        return Value{ErrorType::value};
      }
      return matrix->cells.front();
    }
    return value;
  }

  /// @ref Call::reference.
  [[nodiscard]] Result<Value> reference(const Node &node) {
    const auto known = [&](const CellReference &cell) -> Result<bool> {
      if (cell.document.has_value() ||
          (cell.sheet.has_value() &&
           cell.sheet->find(':') != std::string::npos)) {
        return no_answer(); // another document, or several sheets
      }
      return !cell.sheet.has_value() ||
             m_source->sheet(*cell.sheet).has_value();
    };
    const auto both = [&](const CellReference &from,
                          const CellReference &to) -> Result<bool> {
      const Result<bool> first = known(from);
      if (!first.has_value() || !*first) {
        return first;
      }
      return known(to);
    };
    Result<bool> found = false;
    if (const auto *cell = std::get_if<CellReference>(&node.content)) {
      found = known(*cell);
    } else if (const auto *range = std::get_if<RangeReference>(&node.content)) {
      found = both(range->from, range->to);
    } else if (const auto *name = std::get_if<NameReference>(&node.content)) {
      if (name->document.has_value()) {
        return no_answer();
      }
      const std::optional<std::uint32_t> scope =
          name->sheet.has_value() ? m_source->sheet(*name->sheet)
                                  : std::optional(m_cell.sheet);
      found =
          scope.has_value() && m_source->name(name->name, *scope).has_value();
    } else if (const auto *binary = std::get_if<BinaryOperation>(&node.content);
               binary != nullptr && binary->op == BinaryOperator::range &&
               node.children.size() == 2) {
      const auto *from = std::get_if<CellReference>(&node.children[0].content);
      const auto *to = std::get_if<CellReference>(&node.children[1].content);
      if (from != nullptr && to != nullptr) {
        found = both(*from, *to);
      }
    }
    if (!found.has_value()) {
      return std::unexpected(found.error());
    }
    if (!*found) {
      return Value{ErrorType::reference};
    }
    Result<Value> result = value(node);
    if (result.has_value() && !result->holds<Reference>() &&
        !result->holds<ErrorType>()) {
      return Value{ErrorType::reference}; // a name that stands for a value
    }
    return result;
  }

  /// @ref Call::array of an argument evaluated ahead.
  [[nodiscard]] Result<Matrix> array_of(Value value) const {
    return matrix_of(std::move(value));
  }

  /// Evaluates @p node as an array, expanding range operands as `SUMPRODUCT`
  /// does.
  [[nodiscard]] Result<Matrix> array(const Node &node) {
    const bool outer = m_array;
    m_array = true;
    Result<Value> result = value(node);
    m_array = outer;
    if (!result.has_value()) {
      return std::unexpected(result.error());
    }
    return matrix_of(std::move(*result));
  }

  /// The cell at @p position, without reading one past the extent of its
  /// sheet, which is empty.
  [[nodiscard]] Result<Value> cell_value(const SheetPosition &position) const {
    const TableDimensions extent = m_source->extent(position.sheet);
    if (position.cell.row >= extent.rows ||
        position.cell.column >= extent.columns) {
      return Value{Empty{}};
    }
    return read(position);
  }

  [[nodiscard]] Result<Value> read(const SheetPosition &position) const {
    std::optional<Value> value = m_source->cell(position);
    if (!value.has_value()) {
      return no_answer();
    }
    return std::move(*value);
  }

  [[nodiscard]] Result<void> for_each(
      const Reference &reference,
      const std::function<Result<void>(const SheetPosition &, const Value &)>
          &visit) const {
    const auto visit_value =
        [&](const SheetPosition &position,
            const std::optional<Value> &value) -> Result<void> {
      if (!value.has_value()) {
        return no_answer();
      }
      return visit(position, *value);
    };
    for (const Area &area : reference.areas) {
      if (const Result<void> walked =
              m_source->for_each_cell(area, visit_value);
          !walked.has_value()) {
        return walked;
      }
    }
    return {};
  }

  [[nodiscard]] Result<Number> number(const Value &value) const {
    return std::visit(
        [&]<typename T>(const T &content) -> Result<Number> {
          if constexpr (std::is_same_v<T, Empty>) {
            return Number{0.0};
          } else if constexpr (std::is_same_v<T, double> ||
                               std::is_same_v<T, ErrorType>) {
            return Number{content};
          } else if constexpr (std::is_same_v<T, bool>) {
            return Number{content ? 1.0 : 0.0};
          } else if constexpr (std::is_same_v<T, std::string>) {
            const std::optional<Number> number = number_of_text(content);
            if (!number.has_value()) {
              return no_answer();
            }
            return *number;
          } else {
            return scalar(value).and_then(
                [&](const Value &one) { return number(one); });
          }
        },
        value.content);
  }

  [[nodiscard]] Result<Text> text(const Value &value) const {
    return std::visit(
        [&]<typename T>(const T &content) -> Result<Text> {
          if constexpr (std::is_same_v<T, Empty>) {
            return Text{std::string()};
          } else if constexpr (std::is_same_v<T, double>) {
            const std::optional<std::string> text = text_of_number(content);
            if (!text.has_value()) {
              return no_answer();
            }
            return Text{*text};
          } else if constexpr (std::is_same_v<T, bool>) {
            if (m_settings->dialect == Dialect::libreoffice) {
              return Text{std::string(content ? "1" : "0")};
            }
            return Text{std::string(content ? "TRUE" : "FALSE")};
          } else if constexpr (std::is_same_v<T, std::string> ||
                               std::is_same_v<T, ErrorType>) {
            return Text{content};
          } else {
            return scalar(value).and_then(
                [&](const Value &one) { return text(one); });
          }
        },
        value.content);
  }

private:
  const CellSource *m_source{nullptr};
  const Settings *m_settings{nullptr};
  SheetPosition m_cell;
  /// The node of the whole formula.
  const Node *m_root{nullptr};
  /// Whether an operator reads a range as an array of its cells.
  bool m_array{false};
  /// How many names are being read, one inside the other.
  std::uint32_t m_name_depth{0};

  /// The most names one inside the other, so a name that names itself ends.
  static constexpr std::uint32_t name_limit = 16;

  /// @p value as an array: the cells of a range, an array, or one value.
  [[nodiscard]] Result<Matrix> matrix_of(Value value) const {
    if (auto *matrix = std::get_if<Matrix>(&value.content)) {
      return std::move(*matrix);
    }
    const auto *reference = std::get_if<Reference>(&value.content);
    if (reference == nullptr) {
      return Matrix{1, 1, {std::move(value)}};
    }
    if (reference->areas.size() != 1) {
      return no_answer();
    }
    const Area &area = reference->areas.front();
    // the extent of a sheet cuts a whole column, which the grid does not
    if (area.whole_columns || area.whole_rows) {
      return no_answer();
    }
    const std::uint64_t columns =
        std::uint64_t{area.range.to().column} - area.range.from().column + 1;
    const std::uint64_t rows =
        std::uint64_t{area.range.to().row} - area.range.from().row + 1;
    const Result<std::size_t> count = array_size(columns, rows);
    if (!count.has_value()) {
      return std::unexpected(count.error());
    }
    Matrix result{static_cast<std::uint32_t>(columns),
                  static_cast<std::uint32_t>(rows),
                  {}};
    result.cells.reserve(*count);
    for (std::uint32_t row = 0; row < rows; ++row) {
      for (std::uint32_t column = 0; column < columns; ++column) {
        Result<Value> cell =
            read(SheetPosition(area.sheet, area.range.from().column + column,
                               area.range.from().row + row));
        if (!cell.has_value()) {
          return std::unexpected(cell.error());
        }
        result.cells.push_back(std::move(*cell));
      }
    }
    return result;
  }

  [[nodiscard]] Value value_of(const NumberLiteral &literal, const Node &) {
    return Value{literal.value};
  }
  [[nodiscard]] Value value_of(const StringLiteral &literal, const Node &) {
    return Value{literal.value};
  }
  [[nodiscard]] Value value_of(const BooleanLiteral &literal, const Node &) {
    return Value{literal.value};
  }
  [[nodiscard]] Value value_of(const ErrorLiteral &literal, const Node &) {
    return Value{literal.type};
  }
  [[nodiscard]] Value value_of(const Missing &, const Node &) {
    return Value{Empty{}};
  }
  /// What a name stands for. A name whose references are relative reads
  /// from a base cell the two formats state apart, so it has no answer.
  [[nodiscard]] Result<Value> value_of(const NameReference &reference,
                                       const Node &) {
    if (reference.document.has_value() || m_name_depth >= name_limit) {
      return no_answer();
    }
    const Result<std::uint32_t> scope = sheet_of(reference.sheet, m_cell.sheet);
    if (!scope.has_value()) {
      return std::unexpected(scope.error());
    }
    const std::optional<Node> node = m_source->name(reference.name, *scope);
    if (!node.has_value() || is_relative(*node)) {
      return no_answer();
    }
    ++m_name_depth;
    Result<Value> result = value(*node);
    --m_name_depth;
    return result;
  }

  /// Whether @p node holds a reference that is relative on an axis, or that
  /// names no sheet.
  [[nodiscard]] static bool is_relative(const Node &node) {
    const auto relative = [](const CellReference &cell, const bool first) {
      return (first && !cell.sheet.has_value()) ||
             (cell.column.has_value() && !cell.column->absolute) ||
             (cell.row.has_value() && !cell.row->absolute);
    };
    if (const auto *cell = std::get_if<CellReference>(&node.content)) {
      return relative(*cell, true);
    }
    if (const auto *range = std::get_if<RangeReference>(&node.content)) {
      return relative(range->from, true) || relative(range->to, false);
    }
    return std::ranges::any_of(node.children, is_relative);
  }
  [[nodiscard]] Result<Value> value_of(const CellReference &cell,
                                       const Node &) {
    return area(cell, cell).transform([](const Area &one) {
      return Value{Reference{{one}}};
    });
  }
  [[nodiscard]] Result<Value> value_of(const RangeReference &range,
                                       const Node &) {
    return area(range.from, range.to).transform([](const Area &one) {
      return Value{Reference{{one}}};
    });
  }

  [[nodiscard]] Result<Value> value_of(const FunctionCall &call,
                                       const Node &node) {
    const Function function = find_function(call.name);
    if (function == nullptr) {
      return no_answer();
    }
    if (m_array) {
      return mapped(function, find_array_rule(call.name), node);
    }
    return called(function, Call(this, &node));
  }

  /// What @p function gives for @p call: an error it stops with is its
  /// result.
  [[nodiscard]] static Result<Value> called(const Function function,
                                            const Call &call) {
    try {
      Result<Value> result = function(call);
      if (result.has_value()) {
        return result;
      }
      if (const std::optional<ErrorType> error = result.error().error;
          error.has_value()) {
        return Value{*error};
      }
      return result;
    } catch (const ErrorResult &result) {
      return Value{result.error};
    }
  }

  /// A function in an array context. An argument @p takes_array names stays
  /// for the function to read whole, and the function maps over the
  /// elements of every other one: a range gives a one-cell reference each.
  [[nodiscard]] Result<Value> mapped(const Function function,
                                     const ArrayRule takes_array,
                                     const Node &node) {
    if (takes_array == nullptr) {
      return no_answer();
    }
    std::vector<std::optional<Value>> given(node.children.size());
    std::uint32_t columns = 1;
    std::uint32_t rows = 1;
    for (std::size_t i = 0; i < node.children.size(); ++i) {
      if (takes_array(i) || node.children[i].holds<Missing>()) {
        continue;
      }
      Result<Value> argument = value(node.children[i]);
      if (!argument.has_value()) {
        return std::unexpected(argument.error());
      }
      const Result<std::pair<std::uint32_t, std::uint32_t>> shape =
          shape_of(*argument);
      if (!shape.has_value()) {
        return std::unexpected(shape.error());
      }
      columns = std::max(columns, shape->first);
      rows = std::max(rows, shape->second);
      given[i] = std::move(*argument);
    }
    const auto call_with =
        [&](const std::vector<std::optional<Value>> &arguments) {
          return called(function, Call(this, &node, &arguments));
        };
    if (columns == 1 && rows == 1) {
      return call_with(given);
    }
    const Result<std::size_t> count = array_size(columns, rows);
    if (!count.has_value()) {
      return std::unexpected(count.error());
    }
    Matrix result{columns, rows, {}};
    result.cells.reserve(*count);
    std::vector<std::optional<Value>> element(given.size());
    for (std::uint32_t row = 0; row < rows; ++row) {
      for (std::uint32_t column = 0; column < columns; ++column) {
        for (std::size_t i = 0; i < given.size(); ++i) {
          if (given[i].has_value()) {
            Result<Value> one = element_of(*given[i], column, row);
            if (!one.has_value()) {
              return std::unexpected(one.error());
            }
            element[i] = std::move(*one);
          }
        }
        Result<Value> cell = call_with(element).and_then(
            [&](Value value) { return one_value(std::move(value)); });
        if (!cell.has_value()) {
          return std::unexpected(cell.error());
        }
        result.cells.push_back(std::move(*cell));
      }
    }
    return Value{std::move(result)};
  }

  /// The columns and rows of an array or a range, 1 by 1 for a scalar.
  [[nodiscard]] static Result<std::pair<std::uint32_t, std::uint32_t>>
  shape_of(const Value &value) {
    if (const auto *matrix = std::get_if<Matrix>(&value.content)) {
      return std::pair{matrix->columns, matrix->rows};
    }
    const auto *reference = std::get_if<Reference>(&value.content);
    if (reference == nullptr) {
      return std::pair<std::uint32_t, std::uint32_t>{1, 1};
    }
    if (reference->areas.size() != 1) {
      return no_answer();
    }
    const Area &area = reference->areas.front();
    if (area.whole_columns || area.whole_rows) {
      return no_answer(); // as tall as the grid of the application
    }
    return std::pair{area.range.to().column - area.range.from().column + 1,
                     area.range.to().row - area.range.from().row + 1};
  }

  /// The element of @p value at (@p column, @p row), as @ref spread places
  /// it: a range gives the reference of one cell.
  [[nodiscard]] static Result<Value> element_of(const Value &value,
                                                const std::uint32_t column,
                                                const std::uint32_t row) {
    if (const auto *matrix = std::get_if<Matrix>(&value.content)) {
      return spread(*matrix, column, row);
    }
    const auto *reference = std::get_if<Reference>(&value.content);
    if (reference == nullptr) {
      return value;
    }
    const Result<std::pair<std::uint32_t, std::uint32_t>> shape =
        shape_of(value);
    if (!shape.has_value()) {
      return std::unexpected(shape.error());
    }
    const auto [columns, rows] = *shape;
    const std::uint32_t c = columns == 1 ? 0 : column;
    const std::uint32_t r = rows == 1 ? 0 : row;
    if (c >= columns || r >= rows) {
      return Value{ErrorType::not_available};
    }
    const Area &area = reference->areas.front();
    const TablePosition at(area.range.from().column + c,
                           area.range.from().row + r);
    return Value{Reference{{Area{area.sheet, TableRange(at, at)}}}};
  }

  /// One element of an array a mapped function gives.
  [[nodiscard]] Result<Value> one_value(Value value) {
    if (const auto *reference = std::get_if<Reference>(&value.content)) {
      if (reference->areas.size() != 1 ||
          reference->areas.front().range.from() !=
              reference->areas.front().range.to()) {
        return Value{ErrorType::value};
      }
      return read(SheetPosition(reference->areas.front().sheet,
                                reference->areas.front().range.from()));
    }
    if (const auto *matrix = std::get_if<Matrix>(&value.content)) {
      if (matrix->cells.size() != 1) {
        return no_answer(); // an array in each element
      }
      return matrix->cells.front();
    }
    return value;
  }

  [[nodiscard]] Result<Value> value_of(const ArrayLiteral &array,
                                       const Node &node) {
    Matrix matrix{array.columns, array.rows, {}};
    const Result<std::size_t> count = array_size(array.columns, array.rows);
    if (!count.has_value()) {
      return std::unexpected(count.error());
    }
    if (*count != node.children.size()) {
      return no_answer();
    }
    matrix.cells.reserve(*count);
    for (const Node &child : node.children) {
      Result<Value> element = value(child).and_then(
          [&](Value one) { return scalar(std::move(one)); });
      if (!element.has_value()) {
        return std::unexpected(element.error());
      }
      matrix.cells.push_back(std::move(*element));
    }
    return Value{std::move(matrix)};
  }

  [[nodiscard]] Result<Value> value_of(const UnaryOperation &operation,
                                       const Node &node) {
    const Result<Value> operand = value(node.children.front());
    if (!operand.has_value()) {
      return std::unexpected(operand.error());
    }
    return elementwise(
        *operand, Value{Empty{}},
        [&](const Value &a, const Value &) -> Result<Value> {
          if (operation.op == UnaryOperator::plus) {
            return a;
          }
          const Result<Number> number = this->number(a);
          if (!number.has_value()) {
            return std::unexpected(number.error());
          }
          if (const auto *error = std::get_if<ErrorType>(&*number)) {
            return Value{*error};
          }
          const double x = std::get<double>(*number);
          return Value{operation.op == UnaryOperator::minus ? -x : x / 100};
        });
  }

  [[nodiscard]] Result<Value> value_of(const BinaryOperation &operation,
                                       const Node &node) {
    const Result<Value> left = value(node.children.front());
    if (!left.has_value()) {
      return std::unexpected(left.error());
    }
    const Result<Value> right = value(node.children.back());
    if (!right.has_value()) {
      return std::unexpected(right.error());
    }
    switch (operation.op) {
    case BinaryOperator::range:
      return range(*left, *right);
    case BinaryOperator::intersect:
      return intersect(*left, *right);
    case BinaryOperator::unite:
      return unite(*left, *right);
    default:
      break;
    }
    const bool last = &node == m_root;
    return elementwise(*left, *right, [&](const Value &a, const Value &b) {
      return binary(operation.op, a, b, last);
    });
  }

  /// The rectangle two corners span, on the sheet the first names.
  [[nodiscard]] Result<Area> area(const CellReference &from,
                                  const CellReference &to) const {
    const Result<std::uint32_t> sheet =
        from.document.has_value() ? external_sheet_of(from, to)
                                  : sheet_of(from.sheet, m_cell.sheet);
    if (!sheet.has_value()) {
      return std::unexpected(sheet.error());
    }
    if (!from.document.has_value() && to.document.has_value()) {
      return no_answer(); // over several sheets
    }
    if (!from.document.has_value() && to.sheet.has_value()) {
      const Result<std::uint32_t> other = sheet_of(to.sheet, *sheet);
      if (!other.has_value()) {
        return std::unexpected(other.error());
      }
      if (*other != *sheet) {
        return no_answer(); // over several sheets
      }
    }
    const TableDimensions extent = m_source->extent(*sheet);
    const auto first = [](const std::optional<Coordinate> &coordinate) {
      return coordinate.has_value() ? coordinate->index : 0;
    };
    const auto last = [](const std::optional<Coordinate> &coordinate,
                         const std::uint32_t count) {
      return coordinate.has_value() ? coordinate->index : last_of(count);
    };
    const std::uint32_t from_column = first(from.column);
    const std::uint32_t to_column = last(to.column, extent.columns);
    const std::uint32_t from_row = first(from.row);
    const std::uint32_t to_row = last(to.row, extent.rows);
    return Area{
        *sheet,
        TableRange(
            {std::min(from_column, to_column), std::min(from_row, to_row)},
            {std::max(from_column, to_column), std::max(from_row, to_row)}),
        !from.row.has_value() || !to.row.has_value(),
        !from.column.has_value() || !to.column.has_value()};
  }

  /// The sheet holding the copy the file caches of the sheet of another
  /// document that both corners name.
  [[nodiscard]] Result<std::uint32_t>
  external_sheet_of(const CellReference &from, const CellReference &to) const {
    if (!from.document.has_value() || !from.sheet.has_value() ||
        (to.document.has_value() && to.document != from.document) ||
        (to.sheet.has_value() &&
         !str::equals_ignore_case(*to.sheet, *from.sheet))) {
      return no_answer();
    }
    const std::optional<std::uint32_t> sheet =
        m_source->external_sheet(*from.document, *from.sheet);
    if (!sheet.has_value()) {
      return no_answer();
    }
    return *sheet;
  }

  [[nodiscard]] Result<std::uint32_t>
  sheet_of(const std::optional<std::string> &name,
           const std::uint32_t unstated) const {
    if (!name.has_value()) {
      return unstated;
    }
    const std::optional<std::uint32_t> sheet = m_source->sheet(*name);
    if (!sheet.has_value()) {
      return no_answer();
    }
    return *sheet;
  }

  /// The cell of a range the formula's row or column crosses.
  [[nodiscard]] Result<Value> intersection(const Reference &reference) const {
    if (reference.areas.size() != 1) {
      return Value{ErrorType::value};
    }
    const Area &area = reference.areas.front();
    const TablePosition &from = area.range.from();
    const TablePosition &to = area.range.to();
    const TablePosition &at = m_cell.cell;
    // a whole column reaches past the extent of its sheet, which `to` states
    const bool one_column = from.column == to.column && !area.whole_rows;
    const bool one_row = from.row == to.row && !area.whole_columns;
    if (one_column && one_row) {
      return read(SheetPosition(area.sheet, from));
    }
    if (one_column && at.row >= from.row &&
        (area.whole_columns || at.row <= to.row)) {
      return cell_value(SheetPosition(area.sheet, from.column, at.row));
    }
    if (one_row && at.column >= from.column &&
        (area.whole_rows || at.column <= to.column)) {
      return cell_value(SheetPosition(area.sheet, at.column, from.row));
    }
    return Value{ErrorType::value};
  }

  /// The single area @p value is, else nothing. A whole column has no
  /// answer: the extent of its sheet cuts it, and the grid does not.
  [[nodiscard]] static Result<std::optional<Area>> area_of(const Value &value) {
    const auto *reference = std::get_if<Reference>(&value.content);
    if (reference == nullptr || reference->areas.size() != 1) {
      return std::optional<Area>();
    }
    const Area &area = reference->areas.front();
    if (area.whole_columns || area.whole_rows) {
      return no_answer();
    }
    return std::optional(area);
  }

  /// `A1:B2` spelled as two references around `:`: the rectangle both span.
  [[nodiscard]] static Result<Value> range(const Value &left,
                                           const Value &right) {
    const Result<std::optional<Area>> a = area_of(left);
    if (!a.has_value()) {
      return std::unexpected(a.error());
    }
    const Result<std::optional<Area>> b = area_of(right);
    if (!b.has_value()) {
      return std::unexpected(b.error());
    }
    if (!a->has_value() || !b->has_value()) {
      return Value{ErrorType::value};
    }
    const Area &from = **a;
    const Area &to = **b;
    if (from.sheet != to.sheet) {
      return no_answer();
    }
    return Value{Reference{{Area{
        from.sheet,
        TableRange({std::min(from.range.from().column, to.range.from().column),
                    std::min(from.range.from().row, to.range.from().row)},
                   {std::max(from.range.to().column, to.range.to().column),
                    std::max(from.range.to().row, to.range.to().row)})}}}};
  }

  /// The cells two areas share, `#NULL!` where they share none.
  [[nodiscard]] static Result<Value> intersect(const Value &left,
                                               const Value &right) {
    const Result<std::optional<Area>> first = area_of(left);
    if (!first.has_value()) {
      return std::unexpected(first.error());
    }
    const Result<std::optional<Area>> second = area_of(right);
    if (!second.has_value()) {
      return std::unexpected(second.error());
    }
    if (!first->has_value() || !second->has_value()) {
      return Value{ErrorType::value};
    }
    const Area &a = **first;
    const Area &b = **second;
    const TablePosition from(
        std::max(a.range.from().column, b.range.from().column),
        std::max(a.range.from().row, b.range.from().row));
    const TablePosition to(std::min(a.range.to().column, b.range.to().column),
                           std::min(a.range.to().row, b.range.to().row));
    if (a.sheet != b.sheet || from.column > to.column || from.row > to.row) {
      return Value{ErrorType::null};
    }
    return Value{Reference{{Area{a.sheet, TableRange(from, to)}}}};
  }

  [[nodiscard]] static Value unite(const Value &left, const Value &right) {
    const auto *a = std::get_if<Reference>(&left.content);
    const auto *b = std::get_if<Reference>(&right.content);
    if (a == nullptr || b == nullptr) {
      return Value{ErrorType::value};
    }
    Reference united = *a;
    united.areas.insert(united.areas.end(), b->areas.begin(), b->areas.end());
    return Value{std::move(united)};
  }

  /// @p operation on two values, element by element where either is an
  /// array. A single row or column of an array stands for every row or column
  /// of the other, and a position past both is `#N/A`.
  template <typename Operation>
  [[nodiscard]] Result<Value> elementwise(const Value &a, const Value &b,
                                          const Operation &operation) const {
    if (m_array && (a.holds<Reference>() || b.holds<Reference>())) {
      const auto expanded = [&](const Value &value) -> Result<Value> {
        if (!value.holds<Reference>()) {
          return value;
        }
        return matrix_of(value).transform(
            [](Matrix matrix) { return Value{std::move(matrix)}; });
      };
      const Result<Value> left = expanded(a);
      if (!left.has_value()) {
        return left;
      }
      const Result<Value> right = expanded(b);
      if (!right.has_value()) {
        return right;
      }
      return elementwise(*left, *right, operation);
    }
    const auto *left = std::get_if<Matrix>(&a.content);
    const auto *right = std::get_if<Matrix>(&b.content);
    if (left == nullptr && right == nullptr) {
      const Result<Value> x = scalar(a);
      if (!x.has_value()) {
        return x;
      }
      const Result<Value> y = scalar(b);
      if (!y.has_value()) {
        return y;
      }
      return operation(*x, *y);
    }
    const auto columns_of = [](const Matrix *matrix) {
      return matrix == nullptr ? 1 : matrix->columns;
    };
    const auto rows_of = [](const Matrix *matrix) {
      return matrix == nullptr ? 1 : matrix->rows;
    };
    const auto element = [&](const Value &value, const Matrix *matrix,
                             const std::uint32_t column,
                             const std::uint32_t row) -> Result<Value> {
      return matrix == nullptr ? scalar(value) : spread(*matrix, column, row);
    };
    Matrix result{std::max(columns_of(left), columns_of(right)),
                  std::max(rows_of(left), rows_of(right)),
                  {}};
    const Result<std::size_t> count = array_size(result.columns, result.rows);
    if (!count.has_value()) {
      return std::unexpected(count.error());
    }
    result.cells.reserve(*count);
    for (std::uint32_t row = 0; row < result.rows; ++row) {
      for (std::uint32_t column = 0; column < result.columns; ++column) {
        const Result<Value> x = element(a, left, column, row);
        if (!x.has_value()) {
          return x;
        }
        const Result<Value> y = element(b, right, column, row);
        if (!y.has_value()) {
          return y;
        }
        Result<Value> cell = operation(*x, *y);
        if (!cell.has_value()) {
          return cell;
        }
        result.cells.push_back(std::move(*cell));
      }
    }
    return Value{std::move(result)};
  }

  /// @p last where @p op is the last operation of the formula.
  [[nodiscard]] Result<Value> binary(const BinaryOperator op, const Value &a,
                                     const Value &b, const bool last) const {
    switch (op) {
    case BinaryOperator::concat:
      return concatenate(a, b);
    case BinaryOperator::equal:
    case BinaryOperator::not_equal:
    case BinaryOperator::less:
    case BinaryOperator::less_equal:
    case BinaryOperator::greater:
    case BinaryOperator::greater_equal:
      return compare(op, a, b);
    default:
      return arithmetic(op, a, b, last);
    }
  }

  [[nodiscard]] Result<Value> arithmetic(const BinaryOperator op,
                                         const Value &a, const Value &b,
                                         const bool last) const {
    const Result<Number> left = number(a);
    if (!left.has_value()) {
      return std::unexpected(left.error());
    }
    if (const auto *error = std::get_if<ErrorType>(&*left)) {
      return Value{*error};
    }
    const Result<Number> right = number(b);
    if (!right.has_value()) {
      return std::unexpected(right.error());
    }
    if (const auto *error = std::get_if<ErrorType>(&*right)) {
      return Value{*error};
    }
    const double x = std::get<double>(*left);
    const double y = std::get<double>(*right);
    Result<double> result = 0.0;
    switch (op) {
    case BinaryOperator::add:
      result = add(x, y, last);
      break;
    case BinaryOperator::subtract:
      result = add(x, -y, last);
      break;
    case BinaryOperator::multiply:
      result = x * y;
      break;
    case BinaryOperator::divide:
      if (y == 0) {
        return Value{ErrorType::division};
      }
      result = x / y;
      break;
    case BinaryOperator::power:
      return power(x, y, m_settings->dialect);
    default:
      return no_answer();
    }
    if (!result.has_value()) {
      return std::unexpected(result.error());
    }
    if (!std::isfinite(*result)) {
      return Value{ErrorType::number};
    }
    return Value{*result};
  }

  /// Adds with LibreOffice cancellation (`rtl::math::approxAdd`). Excel
  /// cancellation depends on parentheses absent from this AST.
  [[nodiscard]] Result<double> add(const double a, const double b,
                                   const bool last) const {
    const double result = a + b;
    if (result == 0 || (a < 0) == (b < 0)) {
      return result;
    }
    if (m_settings->dialect == Dialect::libreoffice) {
      return approximate_add(a, b);
    }
    if (last && nearly_cancels(result, std::max(std::abs(a), std::abs(b)))) {
      return no_answer();
    }
    return result;
  }

  [[nodiscard]] Result<Value> concatenate(const Value &a,
                                          const Value &b) const {
    if (m_settings->dialect == Dialect::libreoffice &&
        (a.holds<bool>() || b.holds<bool>())) {
      if (!m_settings->boolean_word.has_value()) {
        return no_answer();
      }
      if (*m_settings->boolean_word) {
        const auto word = [](const Value &value) {
          return value.holds<bool>()
                     ? Value{std::string(value.get<bool>() ? "TRUE" : "FALSE")}
                     : value;
        };
        return concatenate_texts(word(a), word(b));
      }
    }
    return concatenate_texts(a, b);
  }

  [[nodiscard]] Result<Value> concatenate_texts(const Value &a,
                                                const Value &b) const {
    const Result<Text> left = text(a);
    if (!left.has_value()) {
      return std::unexpected(left.error());
    }
    if (const auto *error = std::get_if<ErrorType>(&*left)) {
      return Value{*error};
    }
    const Result<Text> right = text(b);
    if (!right.has_value()) {
      return std::unexpected(right.error());
    }
    if (const auto *error = std::get_if<ErrorType>(&*right)) {
      return Value{*error};
    }
    std::string result =
        std::get<std::string>(*left) + std::get<std::string>(*right);
    if (utf16_length(result) > text_limit) {
      return no_answer();
    }
    return Value{std::move(result)};
  }

  /// A number sorts before a text, and a text before a boolean. LibreOffice
  /// has no boolean: it is the number 1 or 0. An empty cell is 0, an empty
  /// text or false, whichever the other side is.
  [[nodiscard]] Result<Value> compare(const BinaryOperator op, Value a,
                                      Value b) const {
    if (const auto *error = std::get_if<ErrorType>(&a.content)) {
      return Value{*error};
    }
    if (const auto *error = std::get_if<ErrorType>(&b.content)) {
      return Value{*error};
    }
    if (m_settings->dialect == Dialect::libreoffice) {
      for (Value *side : {&a, &b}) {
        if (const auto *boolean = std::get_if<bool>(&side->content)) {
          side->content = *boolean ? 1.0 : 0.0;
        }
      }
    }
    const auto empty_as = [](const Value &other) -> Value {
      if (other.holds<std::string>()) {
        return Value{std::string()};
      }
      if (other.holds<bool>()) {
        return Value{false};
      }
      return Value{0.0};
    };
    if (a.holds<Empty>() && b.holds<Empty>()) {
      return Value{op == BinaryOperator::equal ||
                   op == BinaryOperator::less_equal ||
                   op == BinaryOperator::greater_equal};
    }
    if (a.holds<Empty>()) {
      a = empty_as(b);
    }
    if (b.holds<Empty>()) {
      b = empty_as(a);
    }

    const auto rank = [](const Value &value) {
      return value.holds<double>() ? 0 : value.holds<std::string>() ? 1 : 2;
    };
    const bool equality =
        op == BinaryOperator::equal || op == BinaryOperator::not_equal;
    std::strong_ordering order = rank(a) <=> rank(b);
    if (order == std::strong_ordering::equal) {
      if (a.holds<double>()) {
        const double x = a.get<double>();
        const double y = b.get<double>();
        const Result<bool> same = same_number(m_settings->dialect, x, y);
        if (!same.has_value()) {
          return std::unexpected(same.error());
        }
        order = *same ? std::strong_ordering::equal
                : (x <=> y) == std::partial_ordering::less
                    ? std::strong_ordering::less
                    : std::strong_ordering::greater;
      } else if (a.holds<std::string>()) {
        const bool sensitive = m_settings->case_sensitive;
        if (equality) {
          const Result<bool> equal = equal_texts(
              a.get<std::string>(), b.get<std::string>(), sensitive);
          if (!equal.has_value()) {
            return std::unexpected(equal.error());
          }
          order =
              *equal ? std::strong_ordering::equal : std::strong_ordering::less;
        } else {
          const Result<std::strong_ordering> texts = order_of_texts(
              a.get<std::string>(), b.get<std::string>(), sensitive);
          if (!texts.has_value()) {
            return std::unexpected(texts.error());
          }
          order = *texts;
        }
      } else {
        order = a.get<bool>() <=> b.get<bool>();
      }
    }

    switch (op) {
    case BinaryOperator::equal:
      return Value{order == 0};
    case BinaryOperator::not_equal:
      return Value{order != 0};
    case BinaryOperator::less:
      return Value{order < 0};
    case BinaryOperator::less_equal:
      return Value{order <= 0};
    case BinaryOperator::greater:
      return Value{order > 0};
    default:
      return Value{order >= 0};
    }
  }
};

Call::Call(Evaluator *evaluator, const Node *node,
           const std::vector<std::optional<Value>> *given)
    : m_evaluator{evaluator}, m_node{node}, m_given{given} {}

std::size_t Call::size() const { return m_node->children.size(); }

bool Call::missing(const std::size_t index) const {
  return index >= size() || m_node->children[index].holds<Missing>();
}

Result<Value> Call::value(const std::size_t index) const {
  if (index >= size()) {
    return Value{Empty{}};
  }
  if (m_given != nullptr) {
    if (const std::optional<Value> &given = (*m_given)[index];
        given.has_value()) {
      return *given;
    }
  }
  return m_evaluator->value(m_node->children[index]);
}

Result<Value> Call::scalar(const std::size_t index) const {
  return value(index).and_then(
      [&](Value value) { return scalar_of(std::move(value)); });
}

Result<Value> Call::scalar_of(Value value) const {
  return m_evaluator->scalar(std::move(value));
}

Result<Matrix> Call::array(const std::size_t index) const {
  if (index >= size()) {
    return Matrix{1, 1, {Value{Empty{}}}};
  }
  if (m_given != nullptr) {
    if (const std::optional<Value> &given = (*m_given)[index];
        given.has_value()) {
      return m_evaluator->array_of(*given);
    }
  }
  return m_evaluator->array(m_node->children[index]);
}

Result<void> Call::for_each_value(
    const std::size_t index,
    const std::function<Result<void>(const Value &, bool)> &visit) const {
  const Result<Value> value = this->value(index);
  if (!value.has_value()) {
    return std::unexpected(value.error());
  }
  if (const auto *reference = std::get_if<Reference>(&value->content)) {
    return for_each(*reference, [&](const SheetPosition &, const Value &cell) {
      return visit(cell, false);
    });
  }
  if (const auto *matrix = std::get_if<Matrix>(&value->content)) {
    for (const Value &cell : matrix->cells) {
      if (Result<void> visited = visit(cell, false); !visited.has_value()) {
        return visited;
      }
    }
    return {};
  }
  return visit(*value, true);
}

Result<void> Call::for_each(
    const Reference &reference,
    const std::function<Result<void>(const SheetPosition &, const Value &)>
        &visit) const {
  return m_evaluator->for_each(reference, visit);
}

Result<Value> Call::cell_value(const SheetPosition &position) const {
  return m_evaluator->cell_value(position);
}

Result<Value> Call::reference(const Node &node) const {
  return m_evaluator->reference(node);
}

Result<Number> Call::number(const Value &value) const {
  return m_evaluator->number(value);
}

Result<Text> Call::text(const Value &value) const {
  return m_evaluator->text(value);
}

const Settings &Call::settings() const { return m_evaluator->settings(); }

const SheetPosition &Call::cell() const { return m_evaluator->cell(); }

} // namespace odr::internal::formula

namespace odr::internal {

formula::Result<formula::Value> formula::power(const double x, const double y,
                                               const Dialect dialect) {
  const bool libreoffice = dialect == Dialect::libreoffice;
  if (x == 0 && y == 0) {
    return libreoffice ? Value{1.0} : Value{ErrorType::number};
  }
  if (x == 0 && y < 0) {
    return Value{libreoffice ? ErrorType::number : ErrorType::division};
  }
  if (x < 0 && y != std::trunc(y)) {
    // LibreOffice takes an odd root of a negative number, Excel does not
    if (libreoffice) {
      return no_answer();
    }
    return Value{ErrorType::number};
  }
  const double result = std::pow(x, y);
  if (!std::isfinite(result)) {
    return Value{ErrorType::number};
  }
  return Value{result};
}

formula::Result<void> formula::CellSource::for_each_cell(
    const Area &area,
    const std::function<Result<void>(
        const SheetPosition &, const std::optional<Value> &)> &visit) const {
  const TableDimensions extent = this->extent(area.sheet);
  if (extent.rows == 0 || extent.columns == 0) {
    return {};
  }
  const std::uint32_t last_row = std::min(area.range.to().row, extent.rows - 1);
  const std::uint32_t last_column =
      std::min(area.range.to().column, extent.columns - 1);
  for (std::uint32_t row = area.range.from().row; row <= last_row; ++row) {
    for (std::uint32_t column = area.range.from().column; column <= last_column;
         ++column) {
      const SheetPosition position(area.sheet, column, row);
      if (Result<void> visited = visit(position, cell(position));
          !visited.has_value()) {
        return visited;
      }
    }
  }
  return {};
}

formula::Result<std::strong_ordering>
formula::order_of_texts(const std::string_view a, const std::string_view b,
                        const bool case_sensitive) {
  if (a == b) {
    return std::strong_ordering::equal;
  }
  const auto known = [](const std::string_view text) {
    return std::ranges::all_of(text, [](const char c) {
      return str::is_ascii_digit(c) || str::is_ascii_letter(c);
    });
  };
  if (!known(a) || !known(b)) {
    return no_answer();
  }
  for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
    if (const char left = str::to_lower(a[i]), right = str::to_lower(b[i]);
        left != right) {
      return left <=> right;
    }
  }
  if (a.size() != b.size()) {
    return a.size() <=> b.size();
  }
  if (case_sensitive) {
    for (std::size_t i = 0; i < a.size(); ++i) {
      if (a[i] != b[i]) {
        // the two differ in case alone here, and lower case sorts first
        return a[i] == str::to_lower(a[i]) ? std::strong_ordering::less
                                           : std::strong_ordering::greater;
      }
    }
  }
  return std::strong_ordering::equal;
}

formula::Result<bool> formula::equal_texts(const std::string_view a,
                                           const std::string_view b,
                                           const bool case_sensitive) {
  if (a == b) {
    return true;
  }
  if (case_sensitive) {
    return false;
  }
  const std::optional<std::string> left = folded(a);
  const std::optional<std::string> right = folded(b);
  // folding the case of the rest of Unicode is for the collator
  if (!left.has_value() || !right.has_value()) {
    return no_answer();
  }
  return *left == *right;
}

std::optional<formula::Value> formula::evaluate(const Node &node,
                                                const SheetPosition &cell,
                                                const CellSource &source,
                                                const Settings &settings) {
  try {
    Evaluator evaluator(&source, &settings, cell, &node);
    Result<Value> result = evaluator.value(node).and_then(
        [&](Value value) { return evaluator.scalar(std::move(value)); });
    if (!result.has_value()) {
      return std::nullopt;
    }
    // a formula reading an empty cell shows 0
    if (result->holds<Empty>()) {
      return Value{0.0};
    }
    if (result->holds<double>() && !std::isfinite(result->get<double>())) {
      return std::nullopt;
    }
    return std::move(*result);
  } catch (const NoAnswer &) {
    return std::nullopt;
  }
}

std::optional<formula::Matrix>
formula::evaluate_array(const Node &node, const SheetPosition &cell,
                        const CellSource &source, const Settings &settings) {
  try {
    Evaluator evaluator(&source, &settings, cell, &node);
    Result<Matrix> result = evaluator.array(node);
    if (!result.has_value()) {
      return std::nullopt;
    }
    for (Value &element : result->cells) {
      // an element reading an empty cell is 0, which LibreOffice shows blank
      if (element.holds<Empty>()) {
        element = Value{0.0};
      } else if (element.holds<double>() &&
                 !std::isfinite(element.get<double>())) {
        return std::nullopt;
      }
    }
    return std::move(*result);
  } catch (const NoAnswer &) {
    return std::nullopt;
  }
}

formula::Value formula::spread(const Matrix &result, const std::uint32_t column,
                               const std::uint32_t row) {
  const std::uint32_t c = result.columns == 1 ? 0 : column;
  const std::uint32_t r = result.rows == 1 ? 0 : row;
  if (c >= result.columns || r >= result.rows) {
    return Value{ErrorType::not_available};
  }
  return result.cells[std::size_t{r} * result.columns + c];
}

} // namespace odr::internal
