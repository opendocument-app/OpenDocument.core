#pragma once

#include <odr/table_position.hpp>

#include <odr/internal/formula/formula_ast.hpp>

#include <optional>

#include <string>
#include <vector>

#include <pugixml.hpp>

namespace odr::internal::ooxml::spreadsheet {

/// A `worksheet` element and the name the workbook gives it.
struct NamedWorksheet final {
  std::string name;
  pugi::xml_node node;
};

/// Where the cell at @p position of the edited sheet sits after @p edit,
/// nothing where the edit removes it.
[[nodiscard]] std::optional<TablePosition>
move_position(const TablePosition &position, const formula::SheetEdit &edit);

/// Moves every formula of @p worksheets, every defined name of @p workbook and
/// every entry of @p calc_chain that the edit moves, as
/// `formula::move_references` does. A formula is written back only where a
/// reference in it moved. A shared group whose members would read something
/// else after the move is written out as one formula per cell
/// (ECMA-376 18.3.1.40).
/// @param edited_sheet_id the `sheetId` the calc chain names the edited sheet
///        by.
void move_workbook_references(pugi::xml_node workbook,
                              const std::vector<NamedWorksheet> &worksheets,
                              pugi::xml_node calc_chain,
                              const std::string &edited_sheet_id,
                              const formula::SheetEdit &edit);

/// Moves the ranges @p worksheet states besides its cells: conditional
/// formats, validations, links, the filter, protected ranges, ignored errors
/// and the view. An element whose range a delete takes completely goes; the
/// view keeps a cell, the first one past the removed rows.
void move_sheet_ranges(pugi::xml_node worksheet,
                       const formula::SheetEdit &edit);

/// Moves the anchors of @p drawing (`xdr:wsDr`) as Excel moves a drawing
/// with its cells: a `twoCell` anchor moves each corner, a `oneCell` one moves
/// its box, an `absolute` one stays (ECMA-376 20.5.2.33). A corner inside the
/// removed rows goes to the edge of the rows that stay.
void move_drawing(pugi::xml_node drawing, const formula::SheetEdit &edit);

/// Moves the comments of @p comments (`comments`) and @p threaded
/// (`ThreadedComments`) and the notes of @p vml that show them. A comment in a
/// removed row goes, with its note.
void move_comments(pugi::xml_node comments, pugi::xml_node threaded,
                   pugi::xml_node vml, const formula::SheetEdit &edit);

} // namespace odr::internal::ooxml::spreadsheet
