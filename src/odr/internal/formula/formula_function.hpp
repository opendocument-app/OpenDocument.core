#pragma once

#include <odr/sheet_position.hpp>

#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_value.hpp>

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <variant>

namespace odr::internal::formula {

class Evaluator;

/// Thrown where the evaluator has no answer. `evaluate` catches it, so a
/// function throws it for an argument it cannot read the way the
/// application would.
struct NoAnswer final {};

/// A number, or the error that stands in for one.
using Number = std::variant<double, ErrorType>;
/// A text, or the error that stands in for one.
using Text = std::variant<std::string, ErrorType>;

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

  /// Calls @p visit for every cell of @p reference up to the extent of its
  /// sheet, row by row, with what the cell holds.
  void for_each(const Reference &reference,
                const std::function<void(const SheetPosition &, const Value &)>
                    &visit) const;

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

/// The function a formula names as @p name, with or without the prefix a
/// format writes in front of it (`_xlfn.`, `COM.MICROSOFT.`). Nothing for one
/// the evaluator does not know.
[[nodiscard]] Function find_function(std::string_view name);

} // namespace odr::internal::formula
