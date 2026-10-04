#pragma once

#include <odr/sheet_position.hpp>

#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_value.hpp>

#include <compare>
#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>

namespace odr::internal::formula {

class Evaluator;

/// Thrown where the evaluator has no answer. `evaluate` catches it, so a
/// function throws it for an argument it cannot read the way the
/// application would.
struct NoAnswer final {};

/// Thrown by a function for an argument that is an error, which is then
/// what the function gives back.
struct ErrorResult final {
  ErrorType error{ErrorType::value};
};

/// What a function is called with. The arguments are evaluated on demand, so
/// `IF` reads only the branch it takes.
class Call final {
public:
  Call(Evaluator *evaluator, const Node *node);

  [[nodiscard]] std::size_t size() const;
  /// Whether argument @p index is left out: the second of `IF(A1;;B1)`.
  [[nodiscard]] bool missing(std::size_t index) const;
  /// Argument @p index. A reference stays one.
  [[nodiscard]] Value value(std::size_t index) const;
  /// Argument @p index read as one value: a range gives the cell the
  /// formula's row or column crosses, an array its first element.
  [[nodiscard]] Value scalar(std::size_t index) const;
  /// @p value read as one value, as @ref scalar reads an argument.
  [[nodiscard]] Value scalar_of(Value value) const;
  /// Argument @p index read in an array context, as `SUMPRODUCT` reads it:
  /// an operator reads every cell of a range.
  [[nodiscard]] Matrix array(std::size_t index) const;

  /// Calls @p visit for every cell of @p reference up to the extent of its
  /// sheet, row by row, with what the cell holds.
  void for_each(const Reference &reference,
                const std::function<void(const SheetPosition &, const Value &)>
                    &visit) const;

  /// What the cell at @p position holds. A cell past the extent of its
  /// sheet is empty.
  [[nodiscard]] Value cell_value(const SheetPosition &position) const;

  [[nodiscard]] Number number(const Value &value) const;
  [[nodiscard]] Text text(const Value &value) const;

  [[nodiscard]] const Settings &settings() const;
  /// The cell whose formula this is.
  [[nodiscard]] const SheetPosition &cell() const;

private:
  Evaluator *m_evaluator{nullptr};
  const Node *m_node{nullptr};
};

using Function = Value (*)(const Call &call);

/// A function as a formula names it, without the prefix of its format.
struct FunctionEntry final {
  std::string_view name;
  Function function{nullptr};
};

/// The order of two texts as both applications sort them: ignoring case
/// first, then, where @p case_sensitive, a lower case letter before its
/// upper case one. @throws NoAnswer for a character other than a letter or a
/// digit, whose place is for the collator of the application.
[[nodiscard]] std::strong_ordering
order_of_texts(std::string_view a, std::string_view b, bool case_sensitive);
/// Whether two texts are equal, without case unless @p case_sensitive.
/// @throws NoAnswer where the case of a letter cannot be folded.
[[nodiscard]] bool equal_texts(std::string_view a, std::string_view b,
                               bool case_sensitive);

/// The sum of @p numbers: as LibreOffice's `KahanSum` adds them, and in
/// Excel exact, with no answer where it nearly cancels.
[[nodiscard]] Value sum_of(const Call &call, std::span<const double> numbers);

/// `x^y`, which LibreOffice and Excel compute apart at 0 and below.
[[nodiscard]] Value power(double x, double y, Dialect dialect);

/// Whether @p call follows LibreOffice's rules.
[[nodiscard]] bool is_libreoffice(const Call &call);

/// An argument a function refuses: @p excel in Excel. LibreOffice states a
/// code of its own (`Err:502`), which a file cannot hold as an error, so it
/// has no answer.
[[nodiscard]] Value refused(const Call &call, ErrorType excel);

/// Fails where @p call does not have @p least to @p most arguments, or
/// leaves one out.
void expect_arguments(const Call &call, std::size_t least, std::size_t most);

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

/// Whether @p node calls a function whose result changes without an input
/// changing (`RAND`, `NOW`), or that decides what it reads only when it runs
/// (`OFFSET`, `INDIRECT`).
[[nodiscard]] bool is_volatile(const Node &node);

/// The function a formula names as @p name, with or without the prefix a
/// format writes in front of it (`_xlfn.`, `COM.MICROSOFT.`). Nothing for one
/// the evaluator does not know.
[[nodiscard]] Function find_function(std::string_view name);

} // namespace odr::internal::formula
