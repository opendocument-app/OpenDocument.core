# `.pptx` (PowerPoint) support — design & open work

The **why**; the feature checklist is in [`README.md`](README.md), the shared
OOXML mechanics (registry/adapter pattern, OPC relationships, encryption) in
[`../AGENTS.md`](../AGENTS.md). **Read-only.**

**Scope.** Read `ppt/presentation.xml` and each slide's shape tree into the
abstract model so the generic renderer lays out positioned frames. Paragraphs,
runs, tables, inline text styling.

## Design decisions

**Parsing follows the slide-id list.** The `Document` ctor loads every
relationship target of `presentation.xml` into an `rId → xml` map (masters,
layouts, theme included — only slides are actually walked). Slide **order = the
document order of `p:sldId` in `p:sldIdLst`** (not filename/rId order); each
slide's `r:id` looks up its part, and parsing descends `p:cSld/p:spTree`.
Dispatch table: `p:sp`→**frame** (shapes are frames), `p:graphicFrame`→frame
(descends `a:graphic/a:graphicData`), `p:txBody`→group, `a:p`→paragraph,
`a:r`→span, `a:t`→text, `a:tbl`→table (columns from `a:tblGrid/a:gridCol` via
`append_column`, rows/cells from `a:tr`/`a:tc`; spans from
`gridSpan`/`rowSpan`, covered cells from `hMerge`/`vMerge`).

**Styles are resolved inline — there is no `StyleRegistry`.** Free functions in
the document read `a:rPr` / `a:pPr` attributes directly (font, size in
hundredth-points, bold/italic/underline/strike/shadow/colour/highlight; align,
`a:ind` margins in twips). The element-parent cascade
(`get_intermediate_style` → `.override()`) is the same shape as ODF/docx but
computed on-demand from the XML with no cached or master/default-style
contribution.

**Frame positioning is EMU-based.** `p:spPr/a:xfrm/a:off` + `a:ext` (`p:xfrm`
for `p:graphicFrame`) give `x/y/width/height` in EMUs; anchor type is always
`at_page`. Slide size comes from `p:presentation/p:sldSz` (ECMA-376 default
10in × 7.5in when absent).

## Module layout

| File (`presentation/`) | Role |
|---|---|
| `ooxml_presentation_document.{hpp,cpp}` | `Document` (loads XML + relationships) + `ElementAdapter`; inline style resolution |
| `ooxml_presentation_parser.{hpp,cpp}` | `ParseContext` (slides map) + tag dispatch; presentation.xml → slides → spTree |
| `ooxml_presentation_element_registry.{hpp,cpp}` | Flat element store + Table/Text side maps |

(No style translation unit.)

## Status & open work

Coverage is in [`README.md`](README.md). Foundational gaps, roughly by value:

1. **No master/layout inheritance.** `slide_master_page` returns empty; master
   and layout parts are loaded into the relationship map but never consulted
   (no inherited placeholders, backgrounds, or styles).
2. **Images not modelled** — no `p:pic`/`a:blip` parser entry; `image_href`
   reads ODF-style `xlink:href` (wrong for pptx `r:embed`).
3. **Table cell styles unresolved.** Tables are wired (grid, spans, covered
   cells, column widths/row heights), but `a:tcPr` (fills, borders, margins)
   is not translated.
4. **Read-only.** `text_set_content` machinery exists but is dormant
   (`element_is_editable` → false); wiring edit + save (mirroring docx) is a
   natural next step.
5. **Listings, comments/annotations** not modelled.
