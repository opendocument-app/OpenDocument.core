# `.pptx` (PowerPoint) support — design & open work

The design of the pptx module. The feature checklist is in
[`README.md`](README.md), the shared OOXML mechanics in
[`../AGENTS.md`](../AGENTS.md).

Scope: read `ppt/presentation.xml` and each slide's shape tree into the
abstract model as positioned frames. Paragraphs, runs, tables, inline text
style. Edit runs, paragraphs and text style, and save.

## Design decisions

**Parsing follows the slide-id list.** Slide order is the document order of
`p:sldId` in `p:sldIdLst`, not filename or rId order. The `Document`
constructor loads only those: each `p:sldId`'s `r:id` resolves through the
relationships of `presentation.xml` into the `rId → xml` map. A package may
relate anything to the presentation, for example a Google Slides export
relates a protobuf blob under `ppt/metadata`, so nothing else is parsed as
xml. Parsing descends `p:cSld/p:spTree`. Dispatch: `p:sp` and
`p:graphicFrame` are frames, `p:txBody` and `a:txBody` groups, `a:p` a
paragraph, `a:r` a span, `a:t` and `a:tab` text, `a:br` a line break, `a:tbl` a
table (columns from `a:tblGrid/a:gridCol` via `append_column`, rows and cells
from `a:tr` and `a:tc`, spans from `gridSpan` and `rowSpan`, covered cells from
`hMerge` and `vMerge`).

**Styles resolve inline. There is no `StyleRegistry`.** Free functions in
`ooxml_presentation_style` read `a:rPr` and `a:pPr` where drawingml puts them,
not where wordprocessingml does: font from `a:latin/@typeface`, size in
hundredth-points, bold, italic, underline, strike, shadow, sub- and superscript
from `@baseline`, alignment from `@algn` ([ECMA-376] 20.1.10.59
`ST_TextAlignType`), `@marL` and `@marR` in EMUs, `a:lnSpc` line height,
`a:spcBef` and `a:spcAft` as top and bottom margins. `a:spcBef` and `a:spcAft`
are taken only in their absolute `a:spcPts` form, because the percent form is
of the text size and css would resolve it against the width. The parent
cascade (`get_intermediate_style`, `.override()`) is computed on demand from
the XML, with no master or default-style contribution.

**Colour goes through the theme.** A pptx states most colour as `a:schemeClr`,
a slot name such as `tx1`, `bg1` or `accent1`. A slot resolves along slide,
layout, master, theme: the theme's `a:clrScheme` holds the colours, the
master's `p:clrMap` says which slot each name stands for, and `ColorScheme`
folds the two. A layout, its master and its theme are read once, not once per
slide. Colour transforms (`a:lumMod`, `a:lumOff`, `a:tint`, `a:shade`,
`a:alpha`, [ECMA-376] 20.1.2.3) are dropped, so a tinted slot renders at full
strength.

**A run colour lands only with its ground.** White text on a coloured master
would vanish on a white page. So `p:bg` is read from the slide, else its
layout, else its master, onto `PageLayout::background_color`, and a shape's
own `p:spPr/a:solidFill` onto the frame. A `p:bg` that is not modelled
(`p:bgRef`, `a:gradFill`, `a:blipFill`) ends that walk. Master and layout
shapes are not drawn (open work 1), so text whose only ground is such a shape
stays unreadable.

**Frames are positioned in EMUs.** `p:spPr/a:xfrm/a:off` and `a:ext` (`p:xfrm`
for `p:graphicFrame`) give x, y, width and height. The anchor type is always
`at_page`. Slide size comes from `p:presentation/p:sldSz`, with the ECMA-376
default of 10in × 7.5in when absent.

**Editing and save.** The dom half is `xml::TreeEditor`, shared with odf and
docx. `text_set_style` cuts the `a:r` around the run and writes the toggles
and the size as `a:rPr` attributes, the colour as `a:solidFill` and the
highlight as `a:highlight`, each at its place in the
`CT_TextCharacterProperties` sequence ([ECMA-376] 21.1.2.3.9). `save`
re-serialises the slide parts and byte-copies the rest. The slides are held by
`r:id`, so `save` keeps the path to `r:id` map to know which part it writes.

## Module layout

| File (`presentation/`) | Role |
|---|---|
| `ooxml_presentation_document.{hpp,cpp}` | `Document` (loads the XML and relationships), the `ElementAdapter`, editing and save |
| `ooxml_presentation_style.{hpp,cpp}` | `ColorScheme` (theme × `p:clrMap`), the layout and master walk, the `a:rPr` and `a:pPr` readers |
| `ooxml_presentation_parser.{hpp,cpp}` | `ParseContext` (slides map) and tag dispatch |
| `ooxml_presentation_element_registry.{hpp,cpp}` | Element store plus the table and text side maps |

## Open work

1. No master or layout inheritance beyond colour. `slide_master_page` returns
   null and neither shape tree is walked, so a placeholder's font, size and
   position and every shape a master or layout draws are missing.
   `a:custGeom` and `a:gradFill` are not modelled.
2. Images are not modelled. There is no `p:pic` or `a:blip` parser entry, and
   `image_href` reads `xlink:href`, which a pptx never carries.
3. Table cell styles: `a:tcPr` (fills, borders, margins) is not translated.
4. Editing covers runs, paragraphs and text style only. No editing of a shape,
   a picture or a table's structure.
5. Listings and comments are not modelled.
