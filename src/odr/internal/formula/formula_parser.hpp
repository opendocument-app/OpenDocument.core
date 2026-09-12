#pragma once

#include <odr/internal/formula/formula_ast.hpp>

#include <optional>
#include <string_view>

namespace odr::internal::formula {

/// The syntax a formula is written in. The two share their expression
/// grammar and differ over how they spell a reference and separate arguments.
enum class Syntax {
  opendocument, ///< OpenFormula, as `table:formula` states it
  ooxml,        ///< the expression an `<f>` holds
};

/// Parses @p formula, with or without the `of:=` prefix. Nothing where it does
/// not parse, so a caller reads no reference out of a formula it cannot read.
[[nodiscard]] std::optional<Node> parse(std::string_view formula,
                                        Syntax syntax);

} // namespace odr::internal::formula
