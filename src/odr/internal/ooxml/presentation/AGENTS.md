# `.pptx` (PowerPoint) support — design & open work

The **why**; the feature checklist is in [`README.md`](README.md), the shared
OOXML mechanics (registry/adapter pattern, OPC relationships, encryption) in
[`../AGENTS.md`](../AGENTS.md). **Read-only.**

**Scope.** Read `ppt/presentation.xml` and each slide's shape tree into the
abstract model so the generic renderer lays out positioned frames. Paragraphs,
runs, tables, inline text styling.

## Design decisions

**Parsing follows the slide-id list.** Slide **order = the document order of
`p:sldId` in `p:sldIdLst`** (not filename/rId order), and that list is also what
the `Document` ctor loads: each `p:sldId`'s `r:id` resolves through
`presentation.xml`'s relationships into the `rId → xml` map, and nothing else
does. Loading *every* relationship target instead is what broke a Google Slides
export — it relates a protobuf blob (`ppt/metadata`) to the presentation, and
parsing that as xml threw `NoXmlFile` before a single slide was read. A package
may relate anything at all; only slides are xml we can use. Parsing then
descends `p:cSld/p:spTree`. Dispatch table: `p:sp`→**frame** (shapes are
frames), `p:graphicFrame`→frame (descends `a:graphic/a:graphicData`),
`p:txBody`→group, `a:p`→paragraph, `a:r`→span, `a:t`→text, `a:br`→line break,
`a:tbl`→table (columns from `a:tblGrid/a:gridCol` via `append_column`,
rows/cells from `a:tr`/`a:tc`; spans from `gridSpan`/`rowSpan`, covered cells
from `hMerge`/`vMerge`).

**Styles are resolved inline — there is no `StyleRegistry`.** Free functions in
`ooxml_presentation_style` read `a:rPr` / `a:pPr` directly: font from `a:latin/@typeface`,
size in hundredth-points, bold/italic/underline/strike/shadow, sub/superscript
from `@baseline`; align from `@algn` ([ECMA-376] 20.1.10.59 `ST_TextAlignType`,
which spells the values differently than wordprocessingml does), `@marL`/`@marR`
margins in EMUs, `a:lnSpc` line height, `a:spcBef`/`a:spcAft` as top and bottom
margins. **Read them where drawingml puts them, not where wordprocessingml
does** — these were `rFonts@ascii`, `@jc` and `a:ind` for a while, none of which
a pptx ever carries, so the properties simply never arrived. `a:spcBef`/`a:spcAft`
are taken only in their absolute `a:spcPts` form: the percent form is of the text
size, which css would resolve against the width instead. The element-parent
cascade (`get_intermediate_style` → `.override()`) is the same shape as ODF/docx
but computed on-demand from the XML with no cached or master/default-style
contribution.

**Colour goes through the theme, and never lands without a ground.** A pptx
states most of its colour as `a:schemeClr`, a *slot* name — `tx1`, `bg1`,
`accent1` — so reading only the literal `a:srgbClr` sees almost nothing. A slot
resolves along **slide → layout → master → theme**: the theme's `a:clrScheme`
holds the colours, the master's `p:clrMap` says which slot each name stands for,
and `ColorScheme` is the two folded together. Layouts are shared, so a layout,
its master and its theme are read once rather than once per slide. Colour
*transforms* — `a:lumMod`, `a:lumOff`, `a:tint`, `a:shade`, `a:alpha`
([ECMA-376] 20.1.2.3) — are dropped, so a tinted slot renders at full strength.

**A run colour is only safe once something paints behind it**, which is why it
lands with the ground and not before: white text on a coloured master would
otherwise vanish on our white page. So `p:bg` is read from the slide, else its
layout, else its master, onto `PageLayout::background_color`, and a shape's own
`p:spPr/a:solidFill` onto the frame. A `p:bg` we do not model — `p:bgRef`,
`a:gradFill`, `a:blipFill` — ends that walk rather than falling through to the
part behind it. Master and layout **shapes** are still not drawn (gap (1)
below), so text a deck puts on one stays unreadable where that shape was its
only ground.

**Frame positioning is EMU-based.** `p:spPr/a:xfrm/a:off` + `a:ext` (`p:xfrm`
for `p:graphicFrame`) give `x/y/width/height` in EMUs; anchor type is always
`at_page`. Slide size comes from `p:presentation/p:sldSz` (ECMA-376 default
10in × 7.5in when absent).

## Module layout

| File (`presentation/`) | Role |
|---|---|
| `ooxml_presentation_document.{hpp,cpp}` | `Document` (loads XML + relationships) + `ElementAdapter` |
| `ooxml_presentation_style.{hpp,cpp}` | `ColorScheme` (theme × `p:clrMap`), the layout/master walk, and the `a:rPr`/`a:pPr` resolution |
| `ooxml_presentation_parser.{hpp,cpp}` | `ParseContext` (slides map) + tag dispatch; presentation.xml → slides → spTree |
| `ooxml_presentation_element_registry.{hpp,cpp}` | Flat element store + Table/Text side maps |

(No style translation unit.)

## Status & open work

Coverage is in [`README.md`](README.md). Foundational gaps, roughly by value:

1. **No master/layout inheritance beyond colour.** `slide_master_page` returns
   empty and neither shape tree is walked, so a placeholder's font, size and
   position, and every shape a master or layout draws — banners, logos, rules —
   are missing. The chain *is* walked now, but only for the theme's colours and
   the background fill. Custom geometry (`a:custGeom`) and gradients
   (`a:gradFill`) are unmodelled, so some grounds cannot be painted even once
   the trees are walked.
2. **Images not modelled** — no `p:pic`/`a:blip` parser entry; `image_href`
   reads ODF-style `xlink:href` (wrong for pptx `r:embed`).
3. **Table cell styles unresolved.** Tables are wired (grid, spans, covered
   cells, column widths/row heights), but `a:tcPr` (fills, borders, margins)
   is not translated.
4. **Read-only.** `text_set_content` machinery exists but is dormant
   (`element_is_editable` → false); wiring edit + save (mirroring docx) is a
   natural next step.
5. **Listings, comments/annotations** not modelled.
