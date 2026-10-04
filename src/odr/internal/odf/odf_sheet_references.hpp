#pragma once

#include <odr/sheet_position.hpp>

#include <odr/internal/formula/formula_ast.hpp>

#include <vector>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::odf {

/// Moves every formula and every cell or range address under @p spreadsheet
/// (`office:spreadsheet`) that names a row of the edited sheet, as
/// `formula::move_rows` does. A formula is written back only where a
/// reference in it moved.
/// @return The formula cells whose result the edit can change, by their
///         position after it: the ones reading a removed row, or a range an
///         insert grows.
[[nodiscard]] std::vector<SheetPosition>
move_row_references(pugi::xml_node spreadsheet, const formula::RowEdit &edit);

} // namespace odr::internal::odf
