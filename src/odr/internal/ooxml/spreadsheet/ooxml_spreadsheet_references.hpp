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

/// Moves worksheet formulas, defined names and the calc chain. Expands shared
/// groups if shifting would change their meaning (ECMA-376 18.3.1.40).
/// @param edited_sheet_id The edited sheetId used by the calc chain.
void move_workbook_references(pugi::xml_node workbook,
                              const std::vector<NamedWorksheet> &worksheets,
                              pugi::xml_node calc_chain,
                              const std::string &edited_sheet_id,
                              const formula::SheetEdit &edit);

/// Moves worksheet ranges and filter columns; removes entries whose ranges
/// are deleted. Views retain the first surviving cell.
void move_sheet_ranges(pugi::xml_node worksheet,
                       const formula::SheetEdit &edit);

/// Moves page breaks, clamping deleted positions to the first surviving row
/// or column and merging duplicates.
void move_breaks(pugi::xml_node worksheet, const formula::SheetEdit &edit);

/// Moves drawing anchors with cells (ECMA-376 20.5.2.33): both corners for
/// `twoCell`, the box for `oneCell`, neither for `absolute`. Deleted corners
/// clamp to surviving edges.
void move_drawing(pugi::xml_node drawing, const formula::SheetEdit &edit);

/// Moves the comments of @p comments (`comments`) and @p threaded
/// (`ThreadedComments`) and the notes of @p vml that show them. A comment in a
/// removed row or column goes, with its note.
void move_comments(pugi::xml_node comments, pugi::xml_node threaded,
                   pugi::xml_node vml, const formula::SheetEdit &edit);

/// Moves chart `c:f` ranges; cached values remain until a reader refreshes
/// them.
void move_chart(pugi::xml_node chart, const formula::SheetEdit &edit);

/// Whether @p ref, a cell or a range of the edited sheet, reaches over an
/// edge of the edit: an insert strictly inside it, or a delete taking part
/// of it.
[[nodiscard]] bool cuts(const std::string &ref, const formula::SheetEdit &edit);

/// Whether the edit would change part of the place of @p pivot
/// (`pivotTableDefinition`), or remove all of it, as Excel refuses to.
[[nodiscard]] bool cuts_pivot(pugi::xml_node pivot,
                              const formula::SheetEdit &edit);

/// Moves the place of @p pivot on the edited sheet.
void move_pivot(pugi::xml_node pivot, const formula::SheetEdit &edit);

/// Whether the edit would remove all of the source @p cache
/// (`pivotCacheDefinition`) reads, which leaves no range to state.
[[nodiscard]] bool loses_pivot_source(pugi::xml_node cache,
                                      const formula::SheetEdit &edit);

/// Moves the source @p cache (`pivotCacheDefinition`) reads, where it names
/// the edited sheet.
void move_pivot_cache(pugi::xml_node cache, const formula::SheetEdit &edit);

/// A header cell an inserted table column needs, and the name it states.
struct TableHeader final {
  TablePosition position;
  std::string name;
};

/// Detects deletion of a table header, totals row or all columns, which
/// requires removing the part (ECMA-376 18.5.1.2).
[[nodiscard]] bool cuts_table(pugi::xml_node table,
                              const formula::SheetEdit &edit);

/// Moves references in table formulas, including references to another sheet.
void move_table_formulas(pugi::xml_node table, const std::string &sheet,
                         const formula::SheetEdit &edit);

/// Moves the table, filter and formulas; adds unique IDs/names for new
/// columns and removes deleted ones.
/// @return New header cells, whose text must match the column names to avoid
/// Excel repairs.
[[nodiscard]] std::vector<TableHeader>
move_table(pugi::xml_node table, const formula::SheetEdit &edit);

} // namespace odr::internal::ooxml::spreadsheet
