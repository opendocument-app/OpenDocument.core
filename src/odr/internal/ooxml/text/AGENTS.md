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
4. **Style stubs**: `resolve_table_row_style_` and `resolve_graphic_style_` are
   empty; table cell width is parsed but not applied; the `w:default="1"` style
   flag is ignored.
5. **Comments / annotations** not modelled.
