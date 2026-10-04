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

/// The last index of an axis that states @p count positions.
std::uint32_t last_of(const std::uint32_t count) {
  return count == 0 ? 0 : count - 1;
}

/// Where a character sorts ignoring case: a digit before a letter. Nothing
/// for one whose place depends on the collation of the application.
std::optional<char> primary_of(const char c) {
  if (str::is_ascii_digit(c)) {
    return c;
  }
  if (str::is_ascii_letter(c)) {
    return str::to_lower(c);
  }
  return std::nullopt;
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

  [[nodiscard]] Value value(const Node &node) {
    return std::visit(
        [&](const auto &content) { return value_of(content, node); },
        node.content);
  }

  /// @p value as one value: a range gives the cell the formula's row or
  /// column crosses, an array its first element.
  [[nodiscard]] Value scalar(Value value) const {
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

  /// Evaluates @p node as an array, expanding range operands as `SUMPRODUCT`
  /// does.
  [[nodiscard]] Matrix array(const Node &node) {
    const bool outer = m_array;
    m_array = true;
    Value result = value(node);
    m_array = outer;
    return matrix_of(std::move(result));
  }

  /// The cell at @p position, without reading one past the extent of its
  /// sheet, which is empty.
  [[nodiscard]] Value cell_value(const SheetPosition &position) const {
    const TableDimensions extent = m_source->extent(position.sheet);
    if (position.cell.row >= extent.rows ||
        position.cell.column >= extent.columns) {
      return Value{Empty{}};
    }
    return read(position);
  }

  [[nodiscard]] Value read(const SheetPosition &position) const {
    std::optional<Value> value = m_source->cell(position);
    if (!value.has_value()) {
      throw NoAnswer{};
    }
    return std::move(*value);
  }

  void for_each(const Reference &reference,
                const std::function<void(const SheetPosition &, const Value &)>
                    &visit) const {
    for (const Area &area : reference.areas) {
      m_source->for_each_cell(area, [&](const SheetPosition &position,
                                        const std::optional<Value> &value) {
        if (!value.has_value()) {
          throw NoAnswer{};
        }
        visit(position, *value);
      });
    }
  }

  [[nodiscard]] Number number(const Value &value) const {
    return std::visit(
        [&]<typename T>(const T &content) -> Number {
          if constexpr (std::is_same_v<T, Empty>) {
            return 0.0;
          } else if constexpr (std::is_same_v<T, double>) {
            return content;
          } else if constexpr (std::is_same_v<T, bool>) {
            return content ? 1.0 : 0.0;
          } else if constexpr (std::is_same_v<T, std::string>) {
            const std::optional<Number> number = number_of_text(content);
            if (!number.has_value()) {
              throw NoAnswer{};
            }
            return *number;
          } else if constexpr (std::is_same_v<T, ErrorType>) {
            return content;
          } else {
            return number(scalar(value));
          }
        },
        value.content);
  }

  [[nodiscard]] Text text(const Value &value) const {
    return std::visit(
        [&]<typename T>(const T &content) -> Text {
          if constexpr (std::is_same_v<T, Empty>) {
            return std::string();
          } else if constexpr (std::is_same_v<T, double>) {
            const std::optional<std::string> text = text_of_number(content);
            if (!text.has_value()) {
              throw NoAnswer{};
            }
            return *text;
          } else if constexpr (std::is_same_v<T, bool>) {
            if (m_settings->dialect == Dialect::libreoffice) {
              return std::string(content ? "1" : "0");
            }
            return std::string(content ? "TRUE" : "FALSE");
          } else if constexpr (std::is_same_v<T, std::string>) {
            return content;
          } else if constexpr (std::is_same_v<T, ErrorType>) {
            return content;
          } else {
            return text(scalar(value));
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

  /// The most cells an array context reads out of one range.
  static constexpr std::size_t array_limit = 1 << 20;

  /// @p value as an array: the cells of a range, an array, or one value.
  [[nodiscard]] Matrix matrix_of(Value value) const {
    if (auto *matrix = std::get_if<Matrix>(&value.content)) {
      return std::move(*matrix);
    }
    const auto *reference = std::get_if<Reference>(&value.content);
    if (reference == nullptr) {
      return Matrix{1, 1, {std::move(value)}};
    }
    if (reference->areas.size() != 1) {
      throw NoAnswer{};
    }
    const Area &area = reference->areas.front();
    // the extent of a sheet cuts a whole column, which the grid does not
    if (area.whole_columns || area.whole_rows) {
      throw NoAnswer{};
    }
    const std::uint32_t columns =
        area.range.to().column - area.range.from().column + 1;
    const std::uint32_t rows = area.range.to().row - area.range.from().row + 1;
    if (std::size_t{columns} * rows > array_limit) {
      throw NoAnswer{};
    }
    Matrix result{columns, rows, {}};
    result.cells.reserve(std::size_t{columns} * rows);
    for (std::uint32_t row = 0; row < rows; ++row) {
      for (std::uint32_t column = 0; column < columns; ++column) {
        result.cells.push_back(
            read(SheetPosition(area.sheet, area.range.from().column + column,
                               area.range.from().row + row)));
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
  [[nodiscard]] Value value_of(const NameReference &reference, const Node &) {
    if (reference.document.has_value() || m_name_depth >= name_limit) {
      throw NoAnswer{};
    }
    const std::uint32_t scope = sheet_of(reference.sheet, m_cell.sheet);
    const std::optional<Node> node = m_source->name(reference.name, scope);
    if (!node.has_value() || is_relative(*node)) {
      throw NoAnswer{};
    }
    ++m_name_depth;
    Value result = value(*node);
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
  [[nodiscard]] Value value_of(const CellReference &cell, const Node &) {
    return Value{Reference{{area(cell, cell)}}};
  }
  [[nodiscard]] Value value_of(const RangeReference &range, const Node &) {
    return Value{Reference{{area(range.from, range.to)}}};
  }

  [[nodiscard]] Value value_of(const FunctionCall &call, const Node &node) {
    const Function function = find_function(call.name);
    // a function in an array context maps over the array, which none here
    // does yet
    if (function == nullptr || m_array) {
      throw NoAnswer{};
    }
    try {
      return function(Call(this, &node));
    } catch (const ErrorResult &result) {
      return Value{result.error};
    }
  }

  [[nodiscard]] Value value_of(const ArrayLiteral &array, const Node &node) {
    Matrix matrix{array.columns, array.rows, {}};
    matrix.cells.reserve(node.children.size());
    for (const Node &child : node.children) {
      matrix.cells.push_back(scalar(value(child)));
    }
    return Value{std::move(matrix)};
  }

  [[nodiscard]] Value value_of(const UnaryOperation &operation,
                               const Node &node) {
    const Value operand = value(node.children.front());
    return elementwise(
        operand, Value{Empty{}}, [&](const Value &a, const Value &) {
          if (operation.op == UnaryOperator::plus) {
            return a;
          }
          const Number number = this->number(a);
          if (const auto *error = std::get_if<ErrorType>(&number)) {
            return Value{*error};
          }
          const double x = std::get<double>(number);
          return Value{operation.op == UnaryOperator::minus ? -x : x / 100};
        });
  }

  [[nodiscard]] Value value_of(const BinaryOperation &operation,
                               const Node &node) {
    const Node &left = node.children.front();
    const Node &right = node.children.back();
    switch (operation.op) {
    case BinaryOperator::range:
      return range(value(left), value(right));
    case BinaryOperator::intersect:
      return intersect(value(left), value(right));
    case BinaryOperator::unite:
      return unite(value(left), value(right));
    default:
      break;
    }
    const bool last = &node == m_root;
    return elementwise(value(left), value(right),
                       [&](const Value &a, const Value &b) {
                         return binary(operation.op, a, b, last);
                       });
  }

  /// The rectangle two corners span, on the sheet the first names.
  [[nodiscard]] Area area(const CellReference &from,
                          const CellReference &to) const {
    if (from.document.has_value() || to.document.has_value()) {
      throw NoAnswer{};
    }
    const std::uint32_t sheet = sheet_of(from.sheet, m_cell.sheet);
    if (to.sheet.has_value() && sheet_of(to.sheet, sheet) != sheet) {
      throw NoAnswer{}; // over several sheets
    }
    const TableDimensions extent = m_source->extent(sheet);
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
        sheet,
        TableRange(
            {std::min(from_column, to_column), std::min(from_row, to_row)},
            {std::max(from_column, to_column), std::max(from_row, to_row)}),
        !from.row.has_value() || !to.row.has_value(),
        !from.column.has_value() || !to.column.has_value()};
  }

  [[nodiscard]] std::uint32_t sheet_of(const std::optional<std::string> &name,
                                       const std::uint32_t unstated) const {
    if (!name.has_value()) {
      return unstated;
    }
    const std::optional<std::uint32_t> sheet = m_source->sheet(*name);
    if (!sheet.has_value()) {
      throw NoAnswer{};
    }
    return *sheet;
  }

  /// The cell of a range the formula's row or column crosses.
  [[nodiscard]] Value intersection(const Reference &reference) const {
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
  [[nodiscard]] static std::optional<Area> area_of(const Value &value) {
    const auto *reference = std::get_if<Reference>(&value.content);
    if (reference == nullptr || reference->areas.size() != 1) {
      return std::nullopt;
    }
    const Area &area = reference->areas.front();
    if (area.whole_columns || area.whole_rows) {
      throw NoAnswer{};
    }
    return area;
  }

  /// `A1:B2` spelled as two references around `:`: the rectangle both span.
  [[nodiscard]] static Value range(const Value &left, const Value &right) {
    const std::optional<Area> a = area_of(left);
    const std::optional<Area> b = area_of(right);
    if (!a.has_value() || !b.has_value()) {
      return Value{ErrorType::value};
    }
    if (a->sheet != b->sheet) {
      throw NoAnswer{};
    }
    return Value{Reference{{Area{
        a->sheet,
        TableRange({std::min(a->range.from().column, b->range.from().column),
                    std::min(a->range.from().row, b->range.from().row)},
                   {std::max(a->range.to().column, b->range.to().column),
                    std::max(a->range.to().row, b->range.to().row)})}}}};
  }

  /// The cells two areas share, `#NULL!` where they share none.
  [[nodiscard]] static Value intersect(const Value &left, const Value &right) {
    const std::optional<Area> a = area_of(left);
    const std::optional<Area> b = area_of(right);
    if (!a.has_value() || !b.has_value()) {
      return Value{ErrorType::value};
    }
    const TablePosition from(
        std::max(a->range.from().column, b->range.from().column),
        std::max(a->range.from().row, b->range.from().row));
    const TablePosition to(std::min(a->range.to().column, b->range.to().column),
                           std::min(a->range.to().row, b->range.to().row));
    if (a->sheet != b->sheet || from.column > to.column || from.row > to.row) {
      return Value{ErrorType::null};
    }
    return Value{Reference{{Area{a->sheet, TableRange(from, to)}}}};
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
  [[nodiscard]] Value elementwise(const Value &a, const Value &b,
                                  const Operation &operation) const {
    if (m_array && (a.holds<Reference>() || b.holds<Reference>())) {
      return elementwise(a.holds<Reference>() ? Value{matrix_of(a)} : a,
                         b.holds<Reference>() ? Value{matrix_of(b)} : b,
                         operation);
    }
    const auto *left = std::get_if<Matrix>(&a.content);
    const auto *right = std::get_if<Matrix>(&b.content);
    if (left == nullptr && right == nullptr) {
      return operation(scalar(a), scalar(b));
    }
    const auto columns_of = [](const Matrix *matrix) {
      return matrix == nullptr ? 1 : matrix->columns;
    };
    const auto rows_of = [](const Matrix *matrix) {
      return matrix == nullptr ? 1 : matrix->rows;
    };
    const auto element = [&](const Value &value, const Matrix *matrix,
                             const std::uint32_t column,
                             const std::uint32_t row) -> Value {
      if (matrix == nullptr) {
        return scalar(value);
      }
      const std::uint32_t c = matrix->columns == 1 ? 0 : column;
      const std::uint32_t r = matrix->rows == 1 ? 0 : row;
      if (c >= matrix->columns || r >= matrix->rows) {
        return Value{ErrorType::not_available};
      }
      return matrix->cells[r * matrix->columns + c];
    };
    Matrix result{std::max(columns_of(left), columns_of(right)),
                  std::max(rows_of(left), rows_of(right)),
                  {}};
    if (std::size_t{result.columns} * result.rows > array_limit) {
      throw NoAnswer{};
    }
    result.cells.reserve(std::size_t{result.columns} * result.rows);
    for (std::uint32_t row = 0; row < result.rows; ++row) {
      for (std::uint32_t column = 0; column < result.columns; ++column) {
        result.cells.push_back(operation(element(a, left, column, row),
                                         element(b, right, column, row)));
      }
    }
    return Value{std::move(result)};
  }

  /// @p last where @p op is the last operation of the formula.
  [[nodiscard]] Value binary(const BinaryOperator op, const Value &a,
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

  [[nodiscard]] Value arithmetic(const BinaryOperator op, const Value &a,
                                 const Value &b, const bool last) const {
    const Number left = number(a);
    if (const auto *error = std::get_if<ErrorType>(&left)) {
      return Value{*error};
    }
    const Number right = number(b);
    if (const auto *error = std::get_if<ErrorType>(&right)) {
      return Value{*error};
    }
    const double x = std::get<double>(left);
    const double y = std::get<double>(right);
    double result = 0;
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
      throw NoAnswer{};
    }
    if (!std::isfinite(result)) {
      return Value{ErrorType::number};
    }
    return Value{result};
  }

  /// Adds with LibreOffice cancellation (`rtl::math::approxAdd`). Excel
  /// cancellation depends on parentheses absent from this AST.
  [[nodiscard]] double add(const double a, const double b,
                           const bool last) const {
    const double result = a + b;
    if (result == 0 || (a < 0) == (b < 0)) {
      return result;
    }
    if (m_settings->dialect == Dialect::libreoffice) {
      return approximate_add(a, b);
    }
    if (last && nearly_cancels(result, std::max(std::abs(a), std::abs(b)))) {
      throw NoAnswer{};
    }
    return result;
  }

  [[nodiscard]] Value concatenate(const Value &a, const Value &b) const {
    if (m_settings->dialect == Dialect::libreoffice &&
        (a.holds<bool>() || b.holds<bool>())) {
      if (!m_settings->boolean_word.has_value()) {
        throw NoAnswer{};
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

  [[nodiscard]] Value concatenate_texts(const Value &a, const Value &b) const {
    const Text left = text(a);
    if (const auto *error = std::get_if<ErrorType>(&left)) {
      return Value{*error};
    }
    const Text right = text(b);
    if (const auto *error = std::get_if<ErrorType>(&right)) {
      return Value{*error};
    }
    std::string result =
        std::get<std::string>(left) + std::get<std::string>(right);
    if (utf16_length(result) > text_limit) {
      throw NoAnswer{};
    }
    return Value{std::move(result)};
  }

  /// A number sorts before a text, and a text before a boolean. LibreOffice
  /// has no boolean: it is the number 1 or 0. An empty cell is 0, an empty
  /// text or false, whichever the other side is.
  [[nodiscard]] Value compare(const BinaryOperator op, Value a, Value b) const {
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
        order = same_number(m_settings->dialect, x, y)
                    ? std::strong_ordering::equal
                : x <=> y == std::partial_ordering::less
                    ? std::strong_ordering::less
                    : std::strong_ordering::greater;
      } else if (a.holds<std::string>()) {
        const bool sensitive = m_settings->case_sensitive;
        if (equality) {
          order =
              equal_texts(a.get<std::string>(), b.get<std::string>(), sensitive)
                  ? std::strong_ordering::equal
                  : std::strong_ordering::less;
        } else {
          order = order_of_texts(a.get<std::string>(), b.get<std::string>(),
                                 sensitive);
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

Call::Call(Evaluator *evaluator, const Node *node)
    : m_evaluator{evaluator}, m_node{node} {}

std::size_t Call::size() const { return m_node->children.size(); }

bool Call::missing(const std::size_t index) const {
  return index >= size() || m_node->children[index].holds<Missing>();
}

Value Call::value(const std::size_t index) const {
  if (index >= size()) {
    return Value{Empty{}};
  }
  return m_evaluator->value(m_node->children[index]);
}

Value Call::scalar(const std::size_t index) const {
  return scalar_of(value(index));
}

Value Call::scalar_of(Value value) const {
  return m_evaluator->scalar(std::move(value));
}

Matrix Call::array(const std::size_t index) const {
  if (index >= size()) {
    return Matrix{1, 1, {Value{Empty{}}}};
  }
  return m_evaluator->array(m_node->children[index]);
}

void Call::for_each_value(
    const std::size_t index,
    const std::function<void(const Value &, bool)> &visit) const {
  const Value value = this->value(index);
  if (const auto *reference = std::get_if<Reference>(&value.content)) {
    for_each(*reference, [&](const SheetPosition &, const Value &cell) {
      visit(cell, false);
    });
  } else if (const auto *matrix = std::get_if<Matrix>(&value.content)) {
    for (const Value &cell : matrix->cells) {
      visit(cell, false);
    }
  } else {
    visit(value, true);
  }
}

void Call::for_each(const Reference &reference,
                    const std::function<void(const SheetPosition &,
                                             const Value &)> &visit) const {
  m_evaluator->for_each(reference, visit);
}

Value Call::cell_value(const SheetPosition &position) const {
  return m_evaluator->cell_value(position);
}

Number Call::number(const Value &value) const {
  return m_evaluator->number(value);
}

Text Call::text(const Value &value) const { return m_evaluator->text(value); }

const Settings &Call::settings() const { return m_evaluator->settings(); }

const SheetPosition &Call::cell() const { return m_evaluator->cell(); }

} // namespace odr::internal::formula

namespace odr::internal {

formula::Value formula::power(const double x, const double y,
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
      throw NoAnswer{};
    }
    return Value{ErrorType::number};
  }
  const double result = std::pow(x, y);
  if (!std::isfinite(result)) {
    return Value{ErrorType::number};
  }
  return Value{result};
}

void formula::CellSource::for_each_cell(
    const Area &area,
    const std::function<void(const SheetPosition &,
                             const std::optional<Value> &)> &visit) const {
  const TableDimensions extent = this->extent(area.sheet);
  if (extent.rows == 0 || extent.columns == 0) {
    return;
  }
  const std::uint32_t last_row = std::min(area.range.to().row, extent.rows - 1);
  const std::uint32_t last_column =
      std::min(area.range.to().column, extent.columns - 1);
  for (std::uint32_t row = area.range.from().row; row <= last_row; ++row) {
    for (std::uint32_t column = area.range.from().column; column <= last_column;
         ++column) {
      const SheetPosition position(area.sheet, column, row);
      visit(position, cell(position));
    }
  }
}

std::strong_ordering formula::order_of_texts(const std::string_view a,
                                             const std::string_view b,
                                             const bool case_sensitive) {
  if (a == b) {
    return std::strong_ordering::equal;
  }
  const auto known = [](const std::string_view text) {
    return std::ranges::all_of(
        text, [](const char c) { return primary_of(c).has_value(); });
  };
  if (!known(a) || !known(b)) {
    throw NoAnswer{};
  }
  for (std::size_t i = 0; i < std::min(a.size(), b.size()); ++i) {
    if (const char left = *primary_of(a[i]), right = *primary_of(b[i]);
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

bool formula::equal_texts(const std::string_view a, const std::string_view b,
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
    throw NoAnswer{};
  }
  return *left == *right;
}

std::optional<formula::Value> formula::evaluate(const Node &node,
                                                const SheetPosition &cell,
                                                const CellSource &source,
                                                const Settings &settings) {
  try {
    Evaluator evaluator(&source, &settings, cell, &node);
    Value result = evaluator.scalar(evaluator.value(node));
    // a formula reading an empty cell shows 0
    if (result.holds<Empty>()) {
      return Value{0.0};
    }
    if (result.holds<double>() && !std::isfinite(result.get<double>())) {
      return std::nullopt;
    }
    return result;
  } catch (const NoAnswer &) {
    return std::nullopt;
  }
}

} // namespace odr::internal
