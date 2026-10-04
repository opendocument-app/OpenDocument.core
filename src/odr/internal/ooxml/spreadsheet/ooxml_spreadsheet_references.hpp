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
/// and the view, and the columns a filter counts. An element whose range a
/// delete takes completely goes; the view keeps a cell, the first one past
/// the removed rows or columns.
void move_sheet_ranges(pugi::xml_node worksheet,
                       const formula::SheetEdit &edit);

/// Moves the anchors of @p drawing (`xdr:wsDr`) as Excel moves a drawing
/// with its cells: a `twoCell` anchor moves each corner, a `oneCell` one moves
/// its box, an `absolute` one stays (ECMA-376 20.5.2.33). A corner inside the
/// removed rows or columns goes to the edge of the ones that stay.
void move_drawing(pugi::xml_node drawing, const formula::SheetEdit &edit);

/// Moves the comments of @p comments (`comments`) and @p threaded
/// (`ThreadedComments`) and the notes of @p vml that show them. A comment in a
/// removed row or column goes, with its note.
void move_comments(pugi::xml_node comments, pugi::xml_node threaded,
                   pugi::xml_node vml, const formula::SheetEdit &edit);

/// A header cell an inserted table column needs, and the name it states.
struct TableHeader final {
  TablePosition position;
  std::string name;
};

/// Whether the edit would remove the header row, the totals row or every
/// column of @p table (`table`, ECMA-376 18.5.1.2), which only a removal of
/// the part could answer.
[[nodiscard]] bool cuts_table(pugi::xml_node table,
                              const formula::SheetEdit &edit);

/// Moves @p table, its filter and its formulas with the edit. An inserted
/// column inside it gets a `tableColumn` with the next free `id` and a name
/// no other column has, and a removed one loses its `tableColumn`.
/// @return The header cells of the inserted columns, which have to state
///         their names: Excel repairs a table whose header cells differ.
[[nodiscard]] std::vector<TableHeader>
move_table(pugi::xml_node table, const formula::SheetEdit &edit);

} // namespace odr::internal::ooxml::spreadsheet
