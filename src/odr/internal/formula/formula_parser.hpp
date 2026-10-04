#pragma once

#include <odr/file.hpp>

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

/// The syntax the engine behind @p file_type writes a formula in. Nothing
/// where it states none, or drops the expression at parse time (`.xls`).
[[nodiscard]] std::optional<Syntax> syntax_of(FileType file_type);

/// @p formula without the `of:=` a `table:formula` carries, or the `=` both
/// syntaxes may. The prefix is the producer's namespace, so it is whatever
/// name it declared.
[[nodiscard]] std::string_view strip_prefix(std::string_view formula);

/// Parses @p formula, with or without the `of:=` prefix. Nothing where it does
/// not parse, so a caller reads no reference out of a formula it cannot read.
[[nodiscard]] std::optional<Node> parse(std::string_view formula,
                                        Syntax syntax);

} // namespace odr::internal::formula
