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

**Table merges are resolved in the adapter.** Colspan is `w:tcPr/w:gridSpan`;
a `w:vMerge` continuation (`w:val` absent or "continue") reports the cell as
covered, and the restart cell computes its rowspan by walking following `w:tr`
siblings for a continuation at the same grid column (grid column = sum of
preceding cells' `gridSpan`s).

**Lists are detected structurally**, before the tag table: a paragraph with
`w:pPr/w:numPr` is a list item, nesting synthesised from the `w:ilvl` level —
one list per open level, each nested list hanging off the item that opened it.
A `w:numPr` inherited from `w:pStyle` is *not* seen, so such a paragraph is not
recognised as a list item.

**Numbering resolves at load, not at render.** `NumberingRegistry`
(`ooxml_text_list.*`) indexes `word/numbering.xml`; a post-parse pass walks the
tree in document order and stamps each item with its label. Counters live per
`w:numId` — not per element — which is what makes Word's numbering survive an
interleaved list. `w:lvlText` is the template, `%N` naming a level's counter;
the shared expansion and the number formats are in `common/list_numbering.*`,
where ODF lowers to the same shape.

**Page layout comes from the first `w:sectPr` in document order.** The model
carries one `PageLayout` per text root, while Word carries one per section —
and `w:body/w:sectPr` describes the *last* section, a section being closed by
the `w:pPr/w:sectPr` of its final paragraph. Taking the first one in document
order therefore gives the layout the document opens with, and collapses to the
body's own for the single-section documents that are the norm.

**Style resolution mixes a static hierarchy with a runtime cascade.**
`StyleRegistry` indexes `w:style` by `w:styleId` and pre-flattens the
`w:basedOn` chain: each `Style` recursively resolves its parent, copies the
parent's `ResolvedStyle`, then overlays its own. The default comes from
`w:docDefaults` (`rPrDefault`/`pPrDefault`/…), fallback font-size 12pt.
Partial styles overlay a `wStyle` reference with the element's direct props —
paragraphs additionally fold in the paragraph-mark run props (`w:pPr/w:rPr`).
The *element-tree* cascade is then computed live: `get_intermediate_style` walks
the element parent chain from docDefaults down, `.override()`-ing each partial.
A table resolves its `w:tblStyle` the same way a paragraph resolves its
`w:pStyle`, and contributes the whole resolved style — a table style carries the
paragraph and text properties of everything inside the table, and the cascade is
what carries them down. Its conditional formats (`w:tblStylePr`) are ignored.

**A table's borders are lowered onto its cells**, because css cannot draw an
inside rule from the `<table>`. `table_cell_border` resolves each edge as the
cell's own `w:tcBorders`, then the neighbour's opposite edge, then the table's
`w:tblPr/w:tblBorders` — and returns only the edges the cell *leads*: its top
and left, plus the frame it closes on the last row and column. So a rule between
two cells is one line, whichever of them states it, and `nil` has to read as "no
border" rather than as silence. Word picks the heavier of two competing borders;
here the leading cell's own wins.

**A drawing anchored to the text stays in the text.** `wp:anchor` reports
`AnchorType::at_paragraph` whatever `relativeFrom` says, because css only makes
text flow around a box that is in the flow — hence also a dropped page-relative
offset, and `wrapText="bothSides"`/`"largest"` taking its side from `wp:align`.

**Contextual spacing is decided per paragraph, not per style.**
`w:contextualSpacing` drops the spacing towards a neighbouring paragraph of the
same style, which is what keeps a list tight, so it cannot live in the resolved
style: `partial_paragraph_style` compares the `w:pStyle` of the adjacent `w:p`
siblings and zeroes the margin it applies to. `Style` carries the flag
separately from its `ResolvedStyle` so an inherited one is seen.

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
| `ooxml_text_list.{hpp,cpp}` | `NumberingRegistry`: `word/numbering.xml` index; the post-parse pass that stamps every item's marker |

## Status & open work

Style/element coverage is in [`README.md`](README.md). Foundational gaps:

1. **Numbering gaps.** A `w:numPr` reached through `w:pStyle` is not detected;
   `w:lvlOverride` handles `w:startOverride` and a replacement `w:lvl` but
   nothing else; symbol-font bullets are mapped to Unicode by a small table and
   otherwise fall back to the level's default shape, since the private-use code
   points Word writes render only in Symbol / Wingdings.
2. **No structural editing**; save doesn't stream (buffers document.xml, re-zips
   the whole package); no re-encryption on save.
3. **Theme fonts unhandled.** `w:rFonts w:asciiTheme="minorHAnsi"` (etc.) is
   ignored — only literal `w:ascii` names are read (README example
   `Sample large docx.docx`).
4. **Style stubs**: table cell width is parsed but not applied; the
   `w:default="1"` style flag is ignored. Paragraph
   spacing reads `w:before`/`w:after`/`w:line` but not `w:beforeLines`/
   `w:afterLines`, and drops the value an autospacing flag shadows rather than
   computing what word would. `w:lineRule="atLeast"` lowers to the same fixed
   `line-height` as `exact`, because css has no minimum: right where the value
   exceeds the natural line, tight where a taller font would have grown it.
   Expressing it needs a second field on the public `ParagraphStyle`, since the
   renderer cannot tell the two rules apart from one `Measure`.
5. **Comments / annotations** not modelled.
