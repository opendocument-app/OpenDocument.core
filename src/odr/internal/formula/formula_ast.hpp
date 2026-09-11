#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace odr::internal::formula {

/// The error values a formula states and computes. [OpenFormula] 4.3,
/// [ECMA-376] 18.17.2.4.
enum class ErrorType {
  null,          ///< `#NULL!`
  division,      ///< `#DIV/0!`
  value,         ///< `#VALUE!`
  reference,     ///< `#REF!`
  name,          ///< `#NAME?`
  number,        ///< `#NUM!`
  not_available, ///< `#N/A`
};

enum class UnaryOperator {
  plus,
  minus,
  percent, ///< postfix, and the only one
};

enum class BinaryOperator {
  add,
  subtract,
  multiply,
  divide,
  power,
  concat,
  equal,
  not_equal,
  less,
  less_equal,
  greater,
  greater_equal,
  range,     ///< `A1:B2` where the two sides are not one token
  intersect, ///< `!` in OpenFormula, a space in ooxml, which is not parsed
  unite,     ///< `~` in OpenFormula, `,` in ooxml
};

/// One axis of a reference: the index the file states, and its `$`.
struct Coordinate final {
  std::uint32_t index{0};
  bool absolute{false};

  friend bool operator==(const Coordinate &, const Coordinate &) = default;
};

struct NumberLiteral final {
  double value{0};

  friend bool operator==(const NumberLiteral &,
                         const NumberLiteral &) = default;
};

struct StringLiteral final {
  std::string value;

  friend bool operator==(const StringLiteral &,
                         const StringLiteral &) = default;
};

struct BooleanLiteral final {
  bool value{false};

  friend bool operator==(const BooleanLiteral &,
                         const BooleanLiteral &) = default;
};

struct ErrorLiteral final {
  ErrorType type{ErrorType::null};

  friend bool operator==(const ErrorLiteral &, const ErrorLiteral &) = default;
};

/// One cell. An unstated sheet is the one the formula sits on, and an unstated
/// axis a whole column or row, which only a range spells (`A:A`, `1:1`).
struct CellReference final {
  std::optional<std::string> document{};
  std::optional<std::string> sheet{};
  bool sheet_absolute{false};
  std::optional<Coordinate> column{};
  std::optional<Coordinate> row{};

  friend bool operator==(const CellReference &,
                         const CellReference &) = default;
};

/// `A1:B2`, as the two corners the file spells. The second may name its own
/// sheet.
struct RangeReference final {
  CellReference from;
  CellReference to;

  friend bool operator==(const RangeReference &,
                         const RangeReference &) = default;
};

/// A named expression, left opaque: nothing here resolves what it stands for.
struct NameReference final {
  std::optional<std::string> document{};
  std::optional<std::string> sheet{};
  std::string name{};

  friend bool operator==(const NameReference &,
                         const NameReference &) = default;
};

/// The arguments are the node's children.
struct FunctionCall final {
  std::string name;

  friend bool operator==(const FunctionCall &, const FunctionCall &) = default;
};

/// The operand is the node's only child.
struct UnaryOperation final {
  UnaryOperator op{UnaryOperator::plus};

  friend bool operator==(const UnaryOperation &,
                         const UnaryOperation &) = default;
};

/// The two operands are the node's children.
struct BinaryOperation final {
  BinaryOperator op{BinaryOperator::add};

  friend bool operator==(const BinaryOperation &,
                         const BinaryOperation &) = default;
};

/// An argument the formula leaves out: the second of `IF(A1,,B1)`.
struct Missing final {
  friend bool operator==(const Missing &, const Missing &) = default;
};

/// `{1;2|3;4}` in OpenFormula, `{1,2;3,4}` in ooxml. The elements are the
/// node's children, row by row.
struct ArrayLiteral final {
  std::uint32_t columns{0};
  std::uint32_t rows{0};

  friend bool operator==(const ArrayLiteral &, const ArrayLiteral &) = default;
};

/// One node of a parsed formula. What it holds says what its children are.
struct Node final {
  using Content =
      std::variant<NumberLiteral, StringLiteral, BooleanLiteral, ErrorLiteral,
                   CellReference, RangeReference, NameReference, FunctionCall,
                   UnaryOperation, BinaryOperation, ArrayLiteral, Missing>;

  Content content;
  std::vector<Node> children;

  template <typename T> [[nodiscard]] bool holds() const noexcept {
    return std::holds_alternative<T>(content);
  }
  /// @throws std::bad_variant_access where the node holds something else.
  template <typename T> [[nodiscard]] const T &get() const {
    return std::get<T>(content);
  }
};

/// Moves every relative reference in @p node by (@p columns, @p rows). An
/// absolute (`$`) axis stays, and one moved off the grid becomes `#REF!`.
void shift(Node &node, std::int64_t columns, std::int64_t rows);

} // namespace odr::internal::formula
