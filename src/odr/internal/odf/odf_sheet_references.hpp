#pragma once

#include <odr/sheet_position.hpp>

#include <odr/internal/formula/formula_ast.hpp>

#include <vector>

namespace pugi {
class xml_node;
} // namespace pugi

namespace odr::internal::odf {

/// Moves formulas and addresses under @p spreadsheet, rewriting only changed
/// references.
/// @return Formula positions after the edit whose inputs were deleted or
/// whose ranges grew.
[[nodiscard]] std::vector<SheetPosition>
move_sheet_references(pugi::xml_node spreadsheet,
                      const formula::SheetEdit &edit);

/// Moves the addresses an embedded object's own part (@p object) states: the
/// ranges a chart reads. A chart names the sheet of every range.
void move_object_references(pugi::xml_node object,
                            const formula::SheetEdit &edit);

} // namespace odr::internal::odf
