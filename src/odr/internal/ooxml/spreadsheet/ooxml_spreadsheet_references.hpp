#pragma once

#include <odr/internal/formula/formula_ast.hpp>

#include <string>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

/// A `worksheet` element and the name the workbook gives it.
struct NamedWorksheet final {
  std::string name;
  pugi::xml_node node;
};

/// Moves every formula of @p worksheets, every defined name of @p workbook and
/// every entry of @p calc_chain that names a row of the edited sheet, as
/// `formula::move_rows` does. A formula is written back only where a reference
/// in it moved. A shared group whose members would read something else after
/// the move is written out as one formula per cell (ECMA-376 18.3.1.40).
/// @param edited_sheet_id the `sheetId` the calc chain names the edited sheet
///        by.
void move_row_references(pugi::xml_node workbook,
                         const std::vector<NamedWorksheet> &worksheets,
                         pugi::xml_node calc_chain,
                         const std::string &edited_sheet_id,
                         const formula::RowEdit &edit);

} // namespace odr::internal::ooxml::spreadsheet
