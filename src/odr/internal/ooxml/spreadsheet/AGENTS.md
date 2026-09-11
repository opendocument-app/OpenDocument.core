# `.xlsx` (Excel) support — design & open work

The **why**; the feature checklist is in [`README.md`](README.md), the shared
OOXML mechanics (registry/adapter pattern, OPC relationships, encryption) in
[`../AGENTS.md`](../AGENTS.md). Reads, and writes a cell value.

**Scope.** Read `xl/workbook.xml`, its sheets, the shared-string table and
drawings into the abstract model — a table per sheet. Cell styles resolved from
`xl/styles.xml`.

## Design decisions

**Parsing threads a per-part `ParseContext`.** The ctor pre-parses and caches
every part (`workbook.xml`, each `sheetN.xml` + its `drawing`, `styles.xml`,
`sharedStrings.xml`) into `m_xml_documents_and_relations`, and loads the shared
strings into a positionally-indexed `vector<xml_node>` of `<si>` nodes. Because
relationship targets are relative to the referencing part, a **fresh
`ParseContext` (path + its relations) is built per sheet and per drawing**. Sheet
**order = document order of `<sheet>`** in `workbook.xml`.

**Cells live off-tree in a coordinate map** (mirrors `oldms/spreadsheet`).
`append_sheet_cell` sets only the cell's `parent_id`; it does **not** wire
sibling links, so cells are *not* reachable by child iteration — they're reached
via `Sheet.cells` (`(col,row) → {node, element_id}`). Columns are a
`map<column_max, Column>` keyed by the *max* of a `min..max` span and resolved
with `lookup_greater_or_equals`. Shapes hang off the sheet in a separate chain
(`first_shape_id`). Dimensions come from `<dimension ref>`.

**Values are strings; types are advisory.** A cell with `t="s"` reads `<v>` as
an index into the shared-string table and parses the referenced `<si>`;
otherwise its own `<v>`/`<is>` children. `get_text` concatenates `t` and `v`
nodes verbatim, so a **formula's cached `<v>` result is shown and `<f>` is
never evaluated**. `sheet_cell_value_type` derives number-vs-string from
`c/@t` (default "n" → `float_number` when a `<v>` exists; dates/booleans/errors
report `string`). `sheet_cell_value` adds what that leaves out — `<v>` parsed
as a number where the type is one, and `<f>` as its own string. A shared
formula (18.3.1.40) writes its expression on the group's master alone, so a
member is read **through the master its `si` names** — the parser collects
them per sheet, and `sheet_cell_value` moves the master's expression by the
offset between the two cells (`internal/formula`), `#REF!` where that leaves
the grid. A master that does not parse is handed out as it stands; a member
whose `si` names none stays **set and empty**. Merged ranges from `mergeCells`
land in the `SheetCell` side map as anchor `span` + `is_covered` flags at parse
time.

**Style resolution: styles.xml index vectors.** `StyleRegistry` loads positional
`fonts`/`fills`/`borders`/`cellStyleXfs`/`cellXfs`. A cell's `s` attribute
indexes `cellXfs`; the `xf` picks `fontId`/`fillId`/`borderId`/`alignment`,
each resolved via its index vector. Font/border/alignment are gated by
`applyFont`/`applyBorder`/`applyAlignment`; **fill is applied unconditionally**.
A legacy indexed colour palette is hardcoded. Named-style masters
(`cellStyleXfs`) are loaded but **never consulted** (no master-style
inheritance).

**Writing a cell replaces its children, and a written string goes inline.**
`sheet_set_cell` (position-addressed, so the cell it names need not have an
element) rewrites the `c` and hands the registry a fresh text element for what
it wrote; the elements that read the old children keep their ids and stop being
reachable, which is the tombstoning the editing design asks for. A shared
string is **never** written back into `sharedStrings.xml` — every other cell
indexing that entry would change with it — so the cell becomes
`t="inlineStr"`. Two cells refuse rather than lose something: a covered one and
one holding an `f`.

A position the file writes no `c` for is **stated** rather than refused.
`insert_cell` puts the `c` in its row in column order and, where the file
states no row either, the `row` in `sheetData` in row order, then widens
`dimension` around the new cell. Nothing is reindexed: the cell map is keyed by
position, so an insert touches one entry. A position a merge covers refuses
first, because Excel ignores what a covered `c` holds. That check reads
`mergeCells` again rather than the parsed flags, which only exist for a cell
the file states.

**`save` writes back the parts it can have changed** — every worksheet and
`workbook.xml` — and byte-copies the rest, as `ooxml/text` does for
`document.xml`. pugixml is not asked to parse the declaration, so it cannot
write one back and `save` puts it there itself. Every save sets
`calcPr/@fullCalcOnLoad` (18.2.2): nothing here computes a formula, so the
reader is asked to.

## Module layout

| File (`spreadsheet/`) | Role |
|---|---|
| `ooxml_spreadsheet_document.{hpp,cpp}` | `Document`: caches all parts + shared strings + style registry; `ElementAdapter`; image-href resolution via relationship origin |
| `ooxml_spreadsheet_parser.{hpp,cpp}` | Per-part `ParseContext` + dispatch; sheets/shared-strings/drawings via relationships; cell coordinate-map + shape chain |
| `ooxml_spreadsheet_element_registry.{hpp,cpp}` | Flat element store + Sheet (col/row/cell maps, shape chain) / Text / SheetCell / ElementRelations side maps |
| `ooxml_spreadsheet_style.{hpp,cpp}` | `StyleRegistry`: styles.xml index vectors, `cell_style(i)`, indexed colour palette |

## Status & open work

Coverage is in [`README.md`](README.md). Foundational gaps, roughly by value:

1. **Formulas & rich value types.** `<f>` is never evaluated (the cached `<v>`
   shows); dates, booleans, and errors are typed as plain strings. An array
   formula's members (`t="array"`) carry no `<f>` at all, so they report none.
2. **Content-range detection.** `sheet_content` ignores the requested range and
   returns the full `<dimension>` — no trim to the populated range.
3. **No named/master cell-style inheritance** (`cellStyleXfs` loaded but unused);
   borders rendered as `0.75pt solid` regardless of actual style (`// TODO thin
   only`); cell protection unhandled.
4. **Writing is one cell value.** `sheet_set_cell` writes a number or a string,
   into a cell the file spells or one it states; `text_set_content` throws
   `UnsupportedOperation` — a run inside a cell is not writable. Links and
   comments/annotations not modelled.
