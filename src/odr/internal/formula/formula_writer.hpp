#pragma once

#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_parser.hpp>

#include <optional>
#include <string>
#include <string_view>

namespace odr::internal::formula {

/// Writes @p node back in @p syntax, without the `of:=` prefix or the leading
/// `=`. A parenthesis the precedence already states is dropped.
[[nodiscard]] std::string to_string(const Node &node, Syntax syntax);

/// Moves a space-separated address list, dropping deleted ranges; null if
/// unchanged. Unstated sheets use @p sheet.
/// Addresses omit brackets in ODF ([ODF 1.2] 9.2.5); OOXML uses `sqref`
/// (ECMA-376 18.18.76).
[[nodiscard]] std::optional<std::string>
move_addresses(std::string_view list, const SheetEdit &edit,
               const std::optional<std::string> &sheet, Syntax syntax);

} // namespace odr::internal::formula
