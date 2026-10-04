#pragma once

#include <odr/sheet_position.hpp>

#include <odr/internal/formula/formula_ast.hpp>

#include <vector>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::odf {

/// Moves every formula and every cell or range address under @p spreadsheet
/// (`office:spreadsheet`) that names a row or a column the edit moves, as
/// `formula::move_references` does. A formula is written back only where a
/// reference in it moved.
/// @return The formula cells whose result the edit can change, by their
///         position after it: the ones reading a removed row or column, or a
///         range an insert grows.
[[nodiscard]] std::vector<SheetPosition>
move_sheet_references(pugi::xml_node spreadsheet,
                      const formula::SheetEdit &edit);

/// Moves the addresses an embedded object's own part (@p object) states: the
/// ranges a chart reads. A chart names the sheet of every range.
void move_object_references(pugi::xml_node object,
                            const formula::SheetEdit &edit);

} // namespace odr::internal::odf
