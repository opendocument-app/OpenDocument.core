# `.xlsx` (Excel) support — design & open work

The **why**; the feature checklist is in [`README.md`](README.md), the shared
OOXML mechanics (registry/adapter pattern, OPC relationships, encryption) in
[`../AGENTS.md`](../AGENTS.md). **Read-only.**

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

**Values are always strings.** A cell with `t="s"` reads `<v>` as an index into
the shared-string table and parses the referenced `<si>`; otherwise its own
`<v>`/`<is>` children. `get_text` concatenates `t` and `v` nodes verbatim, so a
**formula's cached `<v>` result is shown and `<f>` is never evaluated**. No
numeric/date/bool typing — `sheet_cell_value_type` is hardcoded to `string`.

**Style resolution: styles.xml index vectors.** `StyleRegistry` loads positional
`fonts`/`fills`/`borders`/`cellStyleXfs`/`cellXfs`. A cell's `s` attribute
indexes `cellXfs`; the `xf` picks `fontId`/`fillId`/`borderId`/`alignment`,
each resolved via its index vector. Font/border/alignment are gated by
`applyFont`/`applyBorder`/`applyAlignment`; **fill is applied unconditionally**.
A legacy indexed colour palette is hardcoded. Named-style masters
(`cellStyleXfs`) are loaded but **never consulted** (no master-style
inheritance).

## Module layout

| File (`spreadsheet/`) | Role |
|---|---|
| `ooxml_spreadsheet_document.{hpp,cpp}` | `Document`: caches all parts + shared strings + style registry; `ElementAdapter`; image-href resolution via relationship origin |
| `ooxml_spreadsheet_parser.{hpp,cpp}` | Per-part `ParseContext` + dispatch; sheets/shared-strings/drawings via relationships; cell coordinate-map + shape chain |
| `ooxml_spreadsheet_element_registry.{hpp,cpp}` | Flat element store + Sheet (col/row/cell maps, shape chain) / Text / SheetCell / ElementRelations side maps |
| `ooxml_spreadsheet_style.{hpp,cpp}` | `StyleRegistry`: styles.xml index vectors, `cell_style(i)`, indexed colour palette |

## Status & open work

Coverage is in [`README.md`](README.md). Foundational gaps, roughly by value:

1. **Cell value types & formulas.** Everything is a string; numeric/date/bool
   typing and formula evaluation are absent (`sheet_cell_value_type` → `string`).
2. **Content-range detection.** `sheet_content` ignores the requested range and
   returns the full `<dimension>` — no trim to the populated range.
3. **Merged cells.** `sheet_cell_span` → `{1,1}`, `sheet_cell_is_covered` →
   false; `mergeCells` not read.
4. **No named/master cell-style inheritance** (`cellStyleXfs` loaded but unused);
   borders rendered as `0.75pt solid` regardless of actual style (`// TODO thin
   only`); cell protection unhandled.
5. **Read-only.** `text_set_content` is a no-op stub; `save` throws. Links and
   comments/annotations not modelled.
