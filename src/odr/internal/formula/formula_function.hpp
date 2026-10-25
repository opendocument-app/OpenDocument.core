#pragma once

#include <odr/sheet_position.hpp>

#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_result.hpp>
#include <odr/internal/formula/formula_value.hpp>

#include <compare>
#include <cstddef>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace odr::internal::formula {

class Evaluator;

/// Signals an indeterminate result; caught by `evaluate`.
struct NoAnswer final {};

/// Propagates an argument error as the function result.
struct ErrorResult final {
  ErrorType error{ErrorType::value};
};

/// The errors one argument holds. Two different ones have no first one both
/// applications agree on.
class Errors final {
public:
  void add(ErrorType error) noexcept;
  /// The error the argument gives, nothing where it holds none. No answer
  /// where it holds two different ones.
  [[nodiscard]] Result<std::optional<ErrorType>> first() const noexcept;

private:
  std::optional<ErrorType> m_first{};
  bool m_mixed{false};
};

/// What a function is called with. The arguments are evaluated on demand, so
/// `IF` reads only the branch it takes.
class Call final {
public:
  /// @p given holds an argument evaluated ahead, as an array context maps a
  /// function over the elements of one.
  Call(Evaluator *evaluator, const Node *node,
       const std::vector<std::optional<Value>> *given = nullptr);

  [[nodiscard]] std::size_t size() const;
  /// Whether argument @p index is left out: the second of `IF(A1;;B1)`.
  [[nodiscard]] bool missing(std::size_t index) const;
  /// Argument @p index. A reference stays one.
  [[nodiscard]] Result<Value> value(std::size_t index) const;
  /// Argument @p index read as one value: a range gives the cell the
  /// formula's row or column crosses, an array its first element.
  [[nodiscard]] Result<Value> scalar(std::size_t index) const;
  /// @p value read as one value, as @ref scalar reads an argument.
  [[nodiscard]] Result<Value> scalar_of(Value value) const;
  /// Argument @p index read in an array context, as `SUMPRODUCT` reads it:
  /// an operator reads every cell of a range.
  [[nodiscard]] Result<Matrix> array(std::size_t index) const;

  /// Calls @p visit with what argument @p index holds: every cell of a range
  /// up to the extent of its sheet, every element of an array, or the
  /// argument itself, which is @p stated. A stop of @p visit ends the walk
  /// and is its result.
  [[nodiscard]] Result<void> for_each_value(
      std::size_t index,
      const std::function<Result<void>(const Value &value, bool stated)> &visit)
      const;

  /// Calls @p visit for every cell of @p reference up to the extent of its
  /// sheet, row by row, with what the cell holds. A stop of @p visit ends the
  /// walk and is its result.
  [[nodiscard]] Result<void> for_each(
      const Reference &reference,
      const std::function<Result<void>(const SheetPosition &, const Value &)>
          &visit) const;

  /// What the cell at @p position holds. A cell past the extent of its
  /// sheet is empty.
  [[nodiscard]] Result<Value> cell_value(const SheetPosition &position) const;
  /// The reference @p node, which a function built at runtime, refers to:
  /// a cell, a range or a name. `#REF!` where it names a sheet or a name the
  /// document does not know, or is no reference.
  [[nodiscard]] Result<Value> reference(const Node &node) const;

  [[nodiscard]] Result<Number> number(const Value &value) const;
  [[nodiscard]] Result<Text> text(const Value &value) const;

  [[nodiscard]] const Settings &settings() const;
  /// The cell whose formula this is.
  [[nodiscard]] const SheetPosition &cell() const;

private:
  Evaluator *m_evaluator{nullptr};
  const Node *m_node{nullptr};
  const std::vector<std::optional<Value>> *m_given{nullptr};
};

using Function = Result<Value> (*)(const Call &call);

// The bridge for a function that still throws its stops goes in step 3.

/// The value of @p result, else its stop thrown as `NoAnswer` or
/// `ErrorResult`.
template <typename T> T require(Result<T> result) {
  if (!result.has_value()) {
    if (const std::optional<ErrorType> error = result.error().error;
        error.has_value()) {
      throw ErrorResult{*error};
    }
    throw NoAnswer{};
  }
  if constexpr (std::is_void_v<T>) {
    return;
  } else {
    return std::move(*result);
  }
}

/// @p function, which throws its stops, as a @ref Function.
template <Value (*function)(const Call &call)>
Result<Value> bridged(const Call &call) {
  return function(call);
}

/// A function as a formula names it, without the prefix of its format.
struct FunctionEntry final {
  std::string_view name{};
  Function function{nullptr};
};

/// Compares ASCII letters and digits, ignoring case first; case-sensitive
/// ties put lowercase first. No answer where ordering requires
/// application-specific collation.
[[nodiscard]] Result<std::strong_ordering>
order_of_texts(std::string_view a, std::string_view b, bool case_sensitive);
/// Whether two texts are equal, without case unless @p case_sensitive. No
/// answer where the case of a letter cannot be folded.
[[nodiscard]] Result<bool> equal_texts(std::string_view a, std::string_view b,
                                       bool case_sensitive);

/// The sum of @p numbers: as LibreOffice's `KahanSum` adds them, and in
/// Excel exact, with no answer where it nearly cancels.
[[nodiscard]] Result<Value> sum_of(const Call &call,
                                   std::span<const double> numbers) noexcept;

/// `x^y`, which LibreOffice and Excel compute apart at 0 and below.
[[nodiscard]] Result<Value> power(double x, double y, Dialect dialect);

/// Whether @p a and @p b are one number in @p dialect: LibreOffice decides
/// as `approxEqual`. Excel's tolerance is not documented, so where
/// `approxEqual` and a reading to 15 digits decide apart, it has no answer.
[[nodiscard]] Result<bool> same_number(Dialect dialect, double a,
                                       double b) noexcept;

/// Whether @p text contains wildcard or regular-expression syntax enabled
/// by the document.
[[nodiscard]] bool is_pattern(const Call &call, std::string_view text) noexcept;

/// Whether @p call follows LibreOffice's rules.
[[nodiscard]] bool is_libreoffice(const Call &call) noexcept;

/// An argument a function refuses: @p excel in Excel. LibreOffice states a
/// code of its own (`Err:502`), which a file cannot hold as an error, so it
/// has no answer.
[[nodiscard]] Result<Value> refused(const Call &call, ErrorType excel) noexcept;

/// Argument @p index as a number. Stops with the error where it is one.
[[nodiscard]] Result<double> number_argument(const Call &call,
                                             std::size_t index) noexcept;

/// No answer where @p call does not have @p least to @p most arguments, or
/// leaves one out.
[[nodiscard]] Result<void> expect_arguments(const Call &call, std::size_t least,
                                            std::size_t most) noexcept;

/// The functions of mathematics and aggregation: `SUM`, `ROUND`, `SIN`.
[[nodiscard]] std::span<const FunctionEntry> math_functions();
/// The functions of logic and information: `IF`, `AND`, `ISBLANK`.
[[nodiscard]] std::span<const FunctionEntry> logic_functions();
/// The functions of text: `LEFT`, `FIND`, `SUBSTITUTE`.
[[nodiscard]] std::span<const FunctionEntry> text_functions();
/// The functions of dates and times: `DATE`, `YEAR`, `WEEKDAY`.
[[nodiscard]] std::span<const FunctionEntry> date_functions();
/// The functions of lookup and the conditional aggregates: `VLOOKUP`,
/// `MATCH`, `COUNTIF`.
[[nodiscard]] std::span<const FunctionEntry> lookup_functions();

/// Whether argument @p index of a function takes a range or an array whole
/// in an array context. Every other argument maps the function over its
/// elements, as `LEN(A1:A3)` gives three lengths.
using ArrayRule = bool (*)(std::size_t index);

/// How the function a formula names as @p name reads an array context. Null
/// for one that has no answer there: `OFFSET`, `INDIRECT` and `CELL`, which
/// would give an array of references.
[[nodiscard]] ArrayRule find_array_rule(std::string_view name);

/// Whether @p node calls a function whose result changes without an input
/// changing (`RAND`, `NOW`), or that decides what it reads only when it runs
/// (`OFFSET`, `INDIRECT`).
[[nodiscard]] bool is_volatile(const Node &node);

/// Whether @p node calls a function that computes a reference at runtime
/// (`OFFSET`, `INDIRECT`).
[[nodiscard]] bool has_dynamic_references(const Node &node);

/// The function a formula names as @p name, with or without the prefix a
/// format writes in front of it (`_xlfn.`, `COM.MICROSOFT.`). Nothing for one
/// the evaluator does not know.
[[nodiscard]] Function find_function(std::string_view name);

} // namespace odr::internal::formula
