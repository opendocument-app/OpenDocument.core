# `.pptx` (PowerPoint) support — design & open work

The **why**; the feature checklist is in [`README.md`](README.md), the shared
OOXML mechanics (registry/adapter pattern, OPC relationships, encryption) in
[`../AGENTS.md`](../AGENTS.md). **Read-only.**

**Scope.** Read `ppt/presentation.xml` and each slide's shape tree into the
abstract model so the generic renderer lays out positioned frames. Paragraphs,
runs, tables (see caveat below), inline text styling.

## Design decisions

**Parsing follows the slide-id list.** The `Document` ctor loads every
relationship target of `presentation.xml` into an `rId → xml` map (masters,
layouts, theme included — only slides are actually walked). Slide **order = the
document order of `p:sldId` in `p:sldIdLst`** (not filename/rId order); each
slide's `r:id` looks up its part, and parsing descends `p:cSld/p:spTree`.
Dispatch table: `p:sp`→**frame** (shapes are frames), `p:txBody`→group,
`a:p`→paragraph, `a:r`→span, `a:t`→text, `a:tbl`→table.

**Styles are resolved inline — there is no `StyleRegistry`.** Free functions in
the document read `a:rPr` / `a:pPr` attributes directly (font, size in
hundredth-points, bold/italic/underline/strike/shadow/colour/highlight; align,
`a:ind` margins in twips). The element-parent cascade
(`get_intermediate_style` → `.override()`) is the same shape as ODF/docx but
computed on-demand from the XML with no cached or master/default-style
contribution.

**Frame positioning is EMU-based.** `p:spPr/a:xfrm/a:off` + `a:ext` give
`x/y/width/height` in EMUs; anchor type is always `at_page`.

## Module layout

| File (`presentation/`) | Role |
|---|---|
| `ooxml_presentation_document.{hpp,cpp}` | `Document` (loads XML + relationships) + `ElementAdapter`; inline style resolution |
| `ooxml_presentation_parser.{hpp,cpp}` | `ParseContext` (slides map) + tag dispatch; presentation.xml → slides → spTree |
| `ooxml_presentation_element_registry.{hpp,cpp}` | Flat element store + Table/Text side maps |

(No style translation unit.)

## Status & open work

Coverage is in [`README.md`](README.md). Foundational gaps, roughly by value:

1. **Slide geometry is hardcoded.** `slide_page_layout` returns a fixed
   11.02in × 8.27in; `slide_master_page` and `slide_name` return empty. Real
   slide size (`p:presentation/p:sldSz`), master/layout inheritance, and slide
   names are all TODO.
2. **Tables are broken.** `a:tbl` is created with the plain builder, not
   `create_table_element`, so no `m_tables` entry exists and `table_first_column`
   throws; `table_dimensions` iterates ODF names (`table:table-column/-row`,
   copy-paste from ODF) → 0×0. Despite the README checkbox, tables need real
   wiring (`create_table_element`/`append_column` + `a:gridCol`/`a:tr`/`a:tc`
   parsers).
3. **Images not modelled** — no `p:pic`/`a:blip` parser entry; `image_href`
   reads ODF-style `xlink:href` (wrong for pptx `r:embed`).
4. **`is_text_node` copy-paste bug.** It matches `w:t`/`w:tab` (wordprocessing)
   instead of `a:t`/`a:tab`, so consecutive `a:t` runs are never coalesced —
   each becomes its own text Element (harmless, but an artifact to fix).
5. **Read-only, inconsistently.** `Document::is_editable`/`save` say no, yet the
   adapter's `element_is_editable` returns true and `text_set_content` is fully
   implemented — the machinery for text editing exists but is not exposed. Wiring
   edit + save (mirroring docx) is a natural next step.
6. **Listings, comments/annotations** not modelled.
