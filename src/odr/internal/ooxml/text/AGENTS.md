# `.docx` (Word) support — design & open work

The design of the docx module. The feature checklist is in
[`README.md`](README.md), the shared OOXML mechanics in
[`../AGENTS.md`](../AGENTS.md).

Scope: read `word/document.xml` into the abstract model, resolve styles from
`word/styles.xml`, edit runs, paragraphs and text style, and save.

## Design decisions

**Parsing is a tag dispatch table.** `parse_tree` walks `w:body`: `w:p` is a
paragraph, `w:r` a span, `w:br` a line break, `w:bookmarkStart` a bookmark,
`w:hyperlink` a link, `w:tbl` a table, `w:sdt` and `w:sdtContent` a group,
`w:drawing` a frame, `a:graphicData` an image. `w:t` and `w:tab` coalesce into
one text element, and `get_text` maps `w:tab` to `\t`.

**Table merges resolve in the adapter.** Colspan is `w:tcPr/w:gridSpan`. A
`w:vMerge` continuation (`w:val` absent or `continue`) is a covered cell. The
restart cell computes its rowspan by walking the following `w:tr` siblings for
a continuation at the same grid column. The grid column is the sum of the
preceding cells' `gridSpan`s.

**Lists are detected structurally**, before the tag table. A paragraph with
`w:pPr/w:numPr` is a list item. The nesting comes from `w:ilvl`: one list per
open level, each nested list hanging off the item that opened it. A `w:numPr`
inherited through `w:pStyle` is not seen.

**Numbering resolves at load.** `NumberingRegistry` (`ooxml_text_list.*`)
indexes `word/numbering.xml`. A post-parse pass walks the tree in document
order and stamps each item with its label. Counters live per `w:numId`, not per
element, so Word's numbering survives an interleaved list. `w:lvlText` is the
template, and `%N` names a level's counter. The expansion and the number
formats are in `common/list_numbering.*`, shared with ODF. The recursive walk
passes its lambda to itself, because NDK 28.1's clang crashes on a recursive
`this auto self` lambda.

**Page layout comes from the first `w:sectPr` in document order.** The model
carries one `PageLayout` per text root. Word carries one per section, and
`w:body/w:sectPr` describes the last section. The first one in document order
is the layout the document opens with.

**Style resolution mixes a static hierarchy with a runtime cascade.**
`StyleRegistry` indexes `w:style` by `w:styleId` and pre-flattens the
`w:basedOn` chain: each `Style` resolves its parent, copies the parent's
`ResolvedStyle`, then overlays its own. The default comes from `w:docDefaults`,
with a fallback font size of 12pt. A partial style overlays a style reference
with the element's direct properties. A paragraph also folds in its
paragraph-mark run properties (`w:pPr/w:rPr`). `get_intermediate_style` then
walks the element parent chain from `docDefaults` down and overrides each
partial. A table resolves `w:tblStyle` the same way and contributes the whole
resolved style, so the cascade carries the table style's paragraph and text
properties down into the cells. `w:tblStylePr` conditional formats are
ignored.

**A table's borders are lowered onto its cells**, because css cannot draw an
inside rule from the `<table>`. `table_cell_border` resolves each edge as the
cell's own `w:tcBorders`, then the neighbour's opposite edge, then
`w:tblPr/w:tblBorders`. It returns only the edges the cell leads: its top and
left, plus the frame it closes on the last row and column. So a rule between
two cells is one line, and `nil` reads as "no border". Where two cells state
competing borders, the leading cell's own wins. Word picks the heavier one.

**A drawing anchored to the text stays in the text.** `wp:anchor` reports
`AnchorType::at_paragraph` whatever `relativeFrom` says, because css only flows
text around a box that is in the flow. So a page-relative offset is dropped,
and `wrapText="bothSides"` or `"largest"` takes its side from `wp:align`.

**Contextual spacing is decided per paragraph.** `w:contextualSpacing` drops
the spacing towards a neighbouring paragraph of the same style.
`partial_paragraph_style` compares the `w:pStyle` of the adjacent `w:p`
siblings and zeroes the margin it applies to. `Style` carries the flag apart
from its `ResolvedStyle`, so an inherited flag is seen.

**Editing and save.** `is_editable` returns true. `text_set_content` tokenises
the new string and splices `w:t` (with `xml:space="preserve"` for spaces) and
`w:tab` nodes into the live `m_document_xml`. `text_insert`, `element_remove`,
`paragraph_split` and `paragraph_merge_next` go through `xml::TreeEditor`,
shared with odf and pptx. `save` re-zips the package and re-serialises only
`word/document.xml`. `save(path, password)` throws.

`text_set_style` cuts the `w:r` around the run (`TreeEditor::isolate`) and
writes the text properties into its `w:rPr`. `CT_RPr` is a sequence Word
enforces, so each property lands at its rank (`run_property_order`) and
replaces an existing one whole. A highlight is `w:highlight` for one of the
sixteen names and `w:shd` otherwise. The reader takes `w:shd` only where no
highlight names a colour ([ECMA-376] 17.3.2.32).

## Module layout

| File (`text/`) | Role |
|---|---|
| `ooxml_text_document.{hpp,cpp}` | `Document`: loads the XML, drives the parse, hosts the `ElementAdapter`, editing and save |
| `ooxml_text_parser.{hpp,cpp}` | `parse_tree`: tag dispatch, list, text and table parsers |
| `ooxml_text_element_registry.{hpp,cpp}` | Element store plus the table and text side maps |
| `ooxml_text_style.{hpp,cpp}` | `StyleRegistry` and `Style`: `w:styleId` index, `w:basedOn` flattening, `docDefaults`, partial-style readers |
| `ooxml_text_list.{hpp,cpp}` | `NumberingRegistry`: `word/numbering.xml` index and the pass that stamps every item's marker |

## Open work

1. Numbering: a `w:numPr` reached through `w:pStyle` is not detected.
   `w:lvlOverride` handles `w:startOverride` and a replacement `w:lvl` only.
   Symbol-font bullets map to Unicode through a small table and otherwise fall
   back to the level's default shape.
2. Save buffers `document.xml` and re-zips the whole package. No re-encryption.
3. Theme fonts: `w:rFonts w:asciiTheme="minorHAnsi"` is ignored. Only literal
   `w:ascii` names are read.
4. Style stubs: `w:tcW` cell width is read and dropped, because it fights the
   column width. `w:default="1"` is ignored. Paragraph spacing reads
   `w:before`, `w:after` and `w:line` but not `w:beforeLines` or
   `w:afterLines`, and drops the value an autospacing flag shadows.
   `w:lineRule="atLeast"` lowers to the same fixed `line-height` as `exact`,
   because css has no minimum line height. A true minimum needs a second field
   on the public `ParagraphStyle`.
5. Comments and annotations are not modelled.
