# Structure editing plan

New odp and pptx files, and edits that add a sheet, a slide from a layout,
or a text box. Shared editing rules are in [editing](editing.md); creation
rules are in [document creation](document-creation.md).

## Stack

| PR | Scope |
|---|---|
| 1 | `create_document` for odp and pptx, with six built-in layouts |
| 1b | pptx placeholders inherit text style and alignment from layout and master |
| 2 | `Document::insert_sheet` for ods and xlsx |
| 3 | `Document::slide_layouts` and `Document::insert_slide` for odp and pptx |
| 4 | `Document::insert_text_box` on a slide or drawing page |
| 5 | envelope ops `insertSheet`, `insertSlide`, `insertTextBox`, and the page side |
| 6 | python, java, swift and npm bindings |

## 1. New presentations

Both formats start with one "Title Slide" slide on a 28 cm × 15.75 cm page,
the size LibreOffice uses for 16:9. One geometry table in hundredths of a
centimetre feeds both packages, so a layout sits at the same place in each
format. A pptx writes the size as `p:sldSz` with no `type`.

| Layout | Placeholders |
|---|---|
| Title Slide | title, subtitle |
| Title and Content | title, content |
| Section Header | title, text |
| Two Content | title, content, content |
| Title Only | title |
| Blank | none |

- odp: `style:presentation-page-layout` elements with a `style:display-name`,
  one master page `Default` with title and outline placeholders, and
  presentation styles for title and outline text.
- pptx: one master, one theme, and one `p:sldLayout` per row. Each layout
  carries its `type` (`title`, `obj`, `secHead`, `twoObj`, `titleOnly`,
  `blank`). The master states `p:txStyles`, and a layout states how a
  placeholder differs from it (`a:lstStyle`), as PowerPoint's layouts do.
  The reader inherits both since 1b.

## 2. Insert a sheet

`Sheet Document::insert_sheet(std::uint32_t index, const std::string &name)`.
An empty name takes the first free `SheetN`. A taken name throws.

- ods: a `table:table` with one column and one row, as a new document has.
- xlsx: a worksheet part, its relationship, its content type, and a
  `sheet` entry with the next free `sheetId`. Sheet-relative state that
  holds a sheet index (`definedName/@localSheetId`, `activeTab`) moves.
- Formulas address a sheet by name, so an insert does not rewrite them.

## 3. Insert a slide from a layout

`std::vector<std::string> Document::slide_layouts()` lists the layouts, and
`Slide Document::insert_slide(std::uint32_t index, std::uint32_t layout)`
adds an empty slide that holds the layout's placeholders.

- pptx: the layouts of every master, in `p:sldLayoutIdLst` order, named by
  `p:cSld/@name`. A slide is a new part with a relationship to its layout,
  plus an entry in `p:sldIdLst`, the presentation's relationships and the
  content types. Each placeholder is copied as a `p:sp` with its `p:ph` and
  an explicit `a:xfrm`, taken from the layout or else from the master's
  placeholder of the same type, because the renderer does not inherit
  geometry yet.
- odp: the `style:presentation-page-layout`s in `styles.xml`, named by
  `style:display-name`, else by the name LibreOffice would show for the
  placeholder set, else by `style:name`. "Blank" is added when no layout is
  empty. The new `draw:page` takes the master page of the slide before it.
  Each placeholder becomes a `draw:frame` with `presentation:class` and a
  `draw:text-box` that holds one empty paragraph.

A placeholder frame holds one empty paragraph, so the editor can put a caret
in it.

## 4. Insert a text box

`Frame Document::insert_text_box(const Element &page, Measure x, Measure y,
Measure width, Measure height)` on a slide or a drawing page. The frame holds
one empty paragraph.

- odf: `draw:frame` with a `draw:text-box`.
- pptx: `p:sp` with `p:cNvSpPr txBox="1"`, a rectangle geometry and a
  `p:txBody`. The shape id is the next free `p:cNvPr/@id` on the slide.

Moving and resizing a frame are not covered.

## 5. Ops and the page

| op | fields |
|---|---|
| `insertSheet` | `index`, `name` |
| `insertSlide` | `index`, `layout`, `id`, `frames`, `paragraphs` |
| `insertTextBox` | `page`, `x`, `y`, `width`, `height`, `id`, `paragraph` |

`frames` and `paragraphs` are negative ids for the created placeholders, in
document order, so later ops can type into them. Sheets keep their positional
address, so an insert renumbers the sheets after it.

The page applies the edit itself ([editing](editing.md) decision 1). An
editable presentation page carries its layouts' placeholder geometry, so the
script can draw a new slide. The new gestures obey the editing scope: under
`paragraph` they refuse with `outOfScope`.

## Validation

`DocumentCreate.*` and the per-feature gtests, then a LibreOffice round trip
(`--convert-to pdf`) of every new and edited package. Keynote opens pptx as a
second reader. PowerPoint and `OpenXmlValidator` are not on this machine.
