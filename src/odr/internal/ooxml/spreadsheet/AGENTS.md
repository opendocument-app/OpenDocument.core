# `.xlsx` (Excel) support — design & open work

The design of the xlsx module. The feature checklist is in
[`README.md`](README.md), the shared OOXML mechanics in
[`../AGENTS.md`](../AGENTS.md). The spreadsheet editing design is in
[`docs/design/spreadsheet-editing.md`](../../../../../docs/design/spreadsheet-editing.md).

Scope: read `xl/workbook.xml`, its sheets, the shared strings and the drawings
into the abstract model, one table per sheet. Cell styles resolve from the
styles part that the workbook relationships name, normally `xl/styles.xml`. A
workbook without one gets an empty styles part, which a save writes with its
relationship and content type. Write a cell value and a cell style, and save.

## Design decisions

**Parsing threads a per-part `ParseContext`.** The constructor parses and
caches every part (`workbook.xml`, each `sheetN.xml` and its drawing,
`styles.xml`, `sharedStrings.xml`) into `m_xml_documents_and_relations`, and
loads the shared strings into a positional vector of `<si>` nodes. A
relationship target is relative to the referencing part, so a fresh
`ParseContext` is built per sheet and per drawing. Sheet order is the document
order of `<sheet>` in `workbook.xml`.

**Cells live off-tree in a coordinate map**, as in `oldms/spreadsheet`.
`append_sheet_cell` sets only the cell's `parent_id` and no sibling links, so
a cell is reached through `Sheet.cells` (`(col, row) → {node, element_id}`),
not by child iteration. Columns are a `map<column_max, Column>` keyed by the
max of a `min..max` span and resolved with `lookup_greater_or_equals`. Shapes
hang off the sheet in their own chain (`first_shape_id`). Dimensions come from
`<dimension ref>`, else from the cells the file states.

**Values.** A cell with `t="s"` reads `<v>` as an index into the shared
strings and parses that `<si>`; otherwise its own `<v>` or `<is>` children.
`sheet_cell_value_type` reads `c/@t`: `b` is a boolean, `e` an error, `d` a
date, `s`, `str` and `inlineStr` a string, and a `<v>` with no type a
`float_number`. `sheet_cell_value` adds the number and the `<f>` expression as
a string. Nothing evaluates a formula. A shared formula ([ECMA-376] 18.3.1.40)
writes its expression on the group's master alone, so a member is read through
the master its `si` names: the parser collects the masters per sheet
(`shared_formulas`), and `sheet_cell_value` moves the master's expression by
the offset between the two cells (`internal/formula`), with `#REF!` where that
leaves the grid. A master that does not parse is handed out as it stands. A
member whose `si` names no master is set and empty. Merged ranges from
`mergeCells` land in the `SheetCell` side map as the anchor's `span` and the
`is_covered` flags at parse time.

**Style resolution is index vectors over `styles.xml`.** `StyleRegistry`
loads `fonts`, `fills`, `borders`, `cellStyleXfs` and `cellXfs`. A cell's `s`
attribute indexes `cellXfs`. The `xf` picks `fontId`, `fillId`, `borderId` and
`alignment`, each resolved via its vector. Font, border and alignment are gated
by `applyFont`, `applyBorder` and `applyAlignment`. Fill is applied
unconditionally, and a pattern paints its `fgColor`. A colour is `rgb`, a slot
of the hardcoded legacy palette, or a `theme` slot of the workbook's
`a:clrScheme`, counted `lt1`, `dk1`, `lt2`, `dk2` first, and `tint` moves it.
`cellStyleXfs` is loaded but never consulted.

**Writing a cell replaces its children, and a written string goes inline.**
`sheet_set_cell` is position-addressed. It rewrites the `c` and hands the
registry a fresh text element. The elements that read the old children keep
their ids and stop being reachable. A shared string is never written back into
`sharedStrings.xml`, because every cell indexing that entry would change with
it, so the cell becomes `t="inlineStr"`. A covered cell, a cell holding an
`f`, an error value, or a date/time without a finite number throws
`UnsupportedOperation`. Dates and times write serials and receive a default
date/time format when the existing format is numeric.

**A cell style is a new `xf`, never an edit of the one the cell names.**
`sheet_set_cell_style` starts from the cell's `s`, else its row's where the
row states `customFormat`, else its column's `style`. `create_cell_format`
copies that `xf` and its `font`, applies the delta, and appends a `font`, a
`fill` and an `xf` only where no equal one exists, with `applyFont`,
`applyFill` and `applyAlignment` set. A `font` keeps the child order of
`CT_Font`. An empty `styleSheet` first gets the entries every `xf` needs.
The styles part is written from its DOM on save. If absent, the constructor
creates it and registers its relationship and content type. Save writes cached
parts even when they were not in the original package.

A position the file states no `c` for is stated by `insert_cell`: the `c` goes
into its row in column order, a missing `row` into `sheetData` in row order,
and `dimension` widens around it. The cell map is keyed by position, so an
insert touches one entry. A covered position refuses first. That check reads
`mergeCells` again, because the parsed flags exist only for a stated cell.

**`save` writes the parts it can have changed**, every worksheet and
`workbook.xml`, and byte-copies the rest. pugixml does not parse the xml
declaration, so `save` writes one itself. Every save sets
`calcPr/@fullCalcOnLoad` ([ECMA-376] 18.2.2) so the reader refreshes formulas
the evaluator cannot compute.

## Module layout

| File (`spreadsheet/`) | Role |
|---|---|
| `ooxml_spreadsheet_document.{hpp,cpp}` | `Document`: part cache, shared strings, style registry, the `ElementAdapter`, cell write and save |
| `ooxml_spreadsheet_parser.{hpp,cpp}` | Per-part `ParseContext` and dispatch; sheets, shared strings, drawings via relationships; the cell map and the shape chain |
| `ooxml_spreadsheet_element_registry.{hpp,cpp}` | Element store plus the Sheet, Text, SheetCell and ElementRelations side maps |
| `ooxml_spreadsheet_style.{hpp,cpp}` | `StyleRegistry`: index vectors, `cell_style(i)`, the indexed colour palette |

## Open work

1. Formula evaluation is partial. `Document::recalculate` computes supported
   stale formulas, and saving after an edit invokes it. Unsupported and array
   formulas retain their cached results. Number formats supply displayed
   numbers, dates and times.
2. `sheet_content` ignores the requested range and returns the full
   `<dimension>`.
3. No `cellStyleXfs` inheritance. Borders render as `0.75pt solid` whatever
   the style. Cell `protection` is read and dropped.
4. `sheet_set_cell` writes numbers, strings, booleans, dates and times. `text_set_content`
   throws `UnsupportedOperation`. A `<hyperlink>` is not modelled, so
   `link_href` is empty. Comments are not modelled.
