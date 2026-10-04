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

/// The cell and range addresses of @p list, split by spaces, moved by
/// @p edit as @ref move_rows moves a reference, the unstated sheet being
/// @p sheet. An address all inside removed rows is left out. Nothing where
/// none moved. An opendocument address is a reference without its brackets
/// ([ODF 1.2] 9.2.5), an ooxml one a `sqref` item (ECMA-376 18.18.76).
[[nodiscard]] std::optional<std::string>
move_row_addresses(std::string_view list, const RowEdit &edit,
                   const std::optional<std::string> &sheet, Syntax syntax);

} // namespace odr::internal::formula
