# `.docx` (Word) support — design & open work

The **why**; the feature checklist is in [`README.md`](README.md), the shared
OOXML mechanics (registry/adapter pattern, OPC relationships, encryption) in
[`../AGENTS.md`](../AGENTS.md). This is the **only editable OOXML format**.

**Scope.** Read `word/document.xml` into the abstract model, resolve styles from
`word/styles.xml`, edit text content, and save. Content coverage (headings,
runs, tables, images, hyperlinks, bookmarks, sdt-as-group, list nesting) per the
README.

## Design decisions

**Parsing: tag dispatch table.** `parse_tree` walks `w:body` via a static
`unordered_map<tag, TreeParser>`: `w:p`→paragraph, `w:r`→span, `w:br`→line_break,
`w:hyperlink`→link, `w:tbl`→table, `w:sdt`/`w:sdtContent`→group,
`w:drawing`→frame, `a:graphicData`→image. `w:t`/`w:tab` coalesce into one text
Element; `get_text` maps `w:tab`→`\t`. Unknown tags are skipped (children still
visited).

**Lists are detected structurally**, before the tag table: a paragraph with
`w:pPr/w:numPr` is a list item, nesting synthesised from the `w:ilvl` level.

**Style resolution mixes a static hierarchy with a runtime cascade.**
`StyleRegistry` indexes `w:style` by `w:styleId` and pre-flattens the
`w:basedOn` chain: each `Style` recursively resolves its parent, copies the
parent's `ResolvedStyle`, then overlays its own. The default comes from
`w:docDefaults` (`rPrDefault`/`pPrDefault`/…), fallback font-size 12pt.
Partial styles overlay a `wStyle` reference with the element's direct props —
paragraphs additionally fold in the paragraph-mark run props (`w:pPr/w:rPr`).
The *element-tree* cascade is then computed live: `get_intermediate_style` walks
the element parent chain from docDefaults down, `.override()`-ing each partial.
Table styles are direct-only (no `w:tblStyle` reference resolution).

**Editing & save.** `is_editable` → true. `text_set_content` tokenises the new
string and splices `w:t` (with `xml:space="preserve"` for spaces) / `w:tab` nodes
into the live `m_document_xml`, updating the registry's node pointers. `save`
re-zips the package, re-serialising **only** `word/document.xml` from the mutated
DOM; everything else is byte-copied. No structural edits; `save(path, password)`
throws (no re-encryption).

## Module layout

| File (`text/`) | Role |
|---|---|
| `ooxml_text_document.{hpp,cpp}` | `Document`: loads XML, drives parse, hosts the `ElementAdapter`, text edit + save |
| `ooxml_text_parser.{hpp,cpp}` | `parse_tree`: tag dispatch, list/text/table special parsers |
| `ooxml_text_element_registry.{hpp,cpp}` | Flat element store + Table/Text side maps |
| `ooxml_text_style.{hpp,cpp}` | `StyleRegistry`/`Style`: `w:styleId` index, `w:basedOn` flatten, docDefaults, partial-style readers |

## Status & open work

Style/element coverage is in [`README.md`](README.md). Foundational gaps:

1. **Numbering.** `w:numPr` levels drive list *nesting*, but `numbering.xml` is
   never parsed, so list formats are not resolved to actual numbers (bullets
   only). The nested-level construction in the parser is partly stubbed
   (`/* TODO fix lists */`).
2. **No structural editing**; save doesn't stream (buffers document.xml, re-zips
   the whole package); no re-encryption on save.
3. **Theme fonts unhandled.** `w:rFonts w:asciiTheme="minorHAnsi"` (etc.) is
   ignored — only literal `w:ascii` names are read (README example
   `Sample large docx.docx`).
4. **Latent copy-paste from the ODF adapter.** The table/cell adapter methods
   query ODF node names (`table:covered-table-cell`, `office:value-type`,
   `table:number-columns-repeated`) that never match docx `w:` nodes, so
   dimension/span/covered-cell logic is effectively dead for docx. Verify and
   rewrite against `w:` names when tables get proper attention.
5. **Style stubs**: `resolve_table_row_style_` and `resolve_graphic_style_` are
   empty; table cell width is parsed but not applied; the `w:default="1"` style
   flag is ignored.
6. **Comments / annotations** not modelled.
