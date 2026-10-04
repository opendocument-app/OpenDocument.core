#include <odr/internal/formula/formula_evaluator.hpp>

#include <odr/internal/formula/formula_function.hpp>
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

/// The order of two texts as both applications sort them: ignoring case
/// first, then, where @p case_sensitive, a lower case letter before its upper
/// case one. Only for letters and digits, the rest is for the collator.
std::strong_ordering order_of_texts(const std::string_view a,
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

/// Whether two texts are equal, without case unless @p case_sensitive.
bool equal_texts(const std::string_view a, const std::string_view b,
                 const bool case_sensitive) {
  if (a == b) {
    return true;
  }
  if (case_sensitive) {
    return false;
  }
  if (str::equals_ignore_case(a, b)) {
    return true;
  }
  const auto ascii = [](const std::string_view text) {
    return std::ranges::all_of(text, [](const char c) {
      return static_cast<unsigned char>(c) < 0x80;
    });
  };
  // folding the case of the rest of Unicode is for the collator
  if (!ascii(a) || !ascii(b)) {
    throw NoAnswer{};
  }
  return false;
}

/// The function a name stands for once the prefix of its format is gone.
std::string canonical_name(std::string_view name) {
  for (const std::string_view prefix :
       {"_xlfn._xlws.", "_xlfn.", "_xlws.", "com.microsoft.", "org.openoffice.",
        "org.libreoffice."}) {
    if (name.size() > prefix.size() &&
        str::equals_ignore_case(name.substr(0, prefix.size()), prefix)) {
      name.remove_prefix(prefix.size());
      break;
    }
  }
  std::string result(name);
  std::ranges::transform(result, result.begin(), str::to_upper);
  return result;
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
      const TableDimensions extent = m_source->extent(area.sheet);
      if (extent.rows == 0 || extent.columns == 0) {
        continue;
      }
      const std::uint32_t last_row =
          std::min(area.range.to().row, extent.rows - 1);
      const std::uint32_t last_column =
          std::min(area.range.to().column, extent.columns - 1);
      for (std::uint32_t row = area.range.from().row; row <= last_row; ++row) {
        for (std::uint32_t column = area.range.from().column;
             column <= last_column; ++column) {
          const SheetPosition position(area.sheet, column, row);
          visit(position, read(position));
        }
      }
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
  [[nodiscard]] Value value_of(const NameReference &, const Node &) {
    throw NoAnswer{};
  }
  [[nodiscard]] Value value_of(const CellReference &cell, const Node &) {
    return Value{Reference{{area(cell, cell)}}};
  }
  [[nodiscard]] Value value_of(const RangeReference &range, const Node &) {
    return Value{Reference{{area(range.from, range.to)}}};
  }

  [[nodiscard]] Value value_of(const FunctionCall &call, const Node &node) {
    const Function function = find_function(call.name);
    if (function == nullptr) {
      throw NoAnswer{};
    }
    return function(Call(this, &node));
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
    return Area{sheet, TableRange({std::min(from_column, to_column),
                                   std::min(from_row, to_row)},
                                  {std::max(from_column, to_column),
                                   std::max(from_row, to_row)})};
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
    if (from == to) {
      return read(SheetPosition(area.sheet, from));
    }
    if (from.column == to.column && at.row >= from.row && at.row <= to.row) {
      return read(SheetPosition(area.sheet, from.column, at.row));
    }
    if (from.row == to.row && at.column >= from.column &&
        at.column <= to.column) {
      return read(SheetPosition(area.sheet, at.column, from.row));
    }
    return Value{ErrorType::value};
  }

  /// The single area @p value is, else nothing.
  [[nodiscard]] static std::optional<Area> area_of(const Value &value) {
    const auto *reference = std::get_if<Reference>(&value.content);
    if (reference == nullptr || reference->areas.size() != 1) {
      return std::nullopt;
    }
    return reference->areas.front();
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
      return power(x, y);
    default:
      throw NoAnswer{};
    }
    if (!std::isfinite(result)) {
      return Value{ErrorType::number};
    }
    return Value{result};
  }

  /// `a + b`. LibreOffice gives 0 where the two cancel to within the
  /// precision of a sheet (`rtl::math::approxAdd`), so `0.1+0.2-0.3` is 0.
  /// Excel does so only for the last operation of a formula outside brackets,
  /// which the tree does not keep, and does not document how near to 0.
  [[nodiscard]] double add(const double a, const double b,
                           const bool last) const {
    const double result = a + b;
    if (result == 0 || (a < 0) == (b < 0)) {
      return result;
    }
    if (m_settings->dialect == Dialect::libreoffice) {
      return approximately_equal(a, -b) ? 0 : result;
    }
    if (last && std::abs(result) < std::max(std::abs(a), std::abs(b)) * 1e-12) {
      throw NoAnswer{};
    }
    return result;
  }

  [[nodiscard]] Value power(const double x, const double y) const {
    const bool libreoffice = m_settings->dialect == Dialect::libreoffice;
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

  [[nodiscard]] Value concatenate(const Value &a, const Value &b) const {
    const Text left = text(a);
    if (const auto *error = std::get_if<ErrorType>(&left)) {
      return Value{*error};
    }
    const Text right = text(b);
    if (const auto *error = std::get_if<ErrorType>(&right)) {
      return Value{*error};
    }
    return Value{std::get<std::string>(left) + std::get<std::string>(right)};
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
        order = approximately_equal(x, y) ? std::strong_ordering::equal
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
  return m_evaluator->scalar(value(index));
}

void Call::for_each(const Reference &reference,
                    const std::function<void(const SheetPosition &,
                                             const Value &)> &visit) const {
  m_evaluator->for_each(reference, visit);
}

Number Call::number(const Value &value) const {
  return m_evaluator->number(value);
}

Text Call::text(const Value &value) const { return m_evaluator->text(value); }

const Settings &Call::settings() const { return m_evaluator->settings(); }

const SheetPosition &Call::cell() const { return m_evaluator->cell(); }

} // namespace odr::internal::formula

namespace odr::internal {

formula::Function formula::find_function(const std::string_view name) {
  const std::string canonical = canonical_name(name);
  if (canonical == "TRUE") {
    return [](const Call &) { return Value{true}; };
  }
  if (canonical == "FALSE") {
    return [](const Call &) { return Value{false}; };
  }
  if (canonical == "NA") {
    return [](const Call &) { return Value{ErrorType::not_available}; };
  }
  return nullptr;
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
    return result;
  } catch (const NoAnswer &) {
    return std::nullopt;
  }
}

} // namespace odr::internal
