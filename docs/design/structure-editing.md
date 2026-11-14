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
  The reader inherits both.

## 2. Insert a sheet

`Sheet Document::insert_sheet(std::uint32_t index, const std::string &name)`.
An empty name takes the first free `SheetN`, counting from the number of
sheets plus one. A name another sheet has, ignoring ASCII case, throws, as
does one with `[]*?:/\`, a leading or trailing apostrophe, or more than 31
characters (Excel's limit, stricter than LibreOffice's).

- ods: a `table:table` with one column and one row, as a new document has.
- xlsx: a worksheet part, its relationship, its content type, and a
  `sheet` entry with the next free `sheetId`. Sheet-relative state that
  holds a sheet index (`definedName/@localSheetId`, `activeTab`) moves.
- Formulas address a sheet by name, so an insert does not rewrite them. The
  dependency graph is dropped and every formula recomputes on the next
  `recalculate`: a formula naming the new sheet resolves now, and a sheet
  range may span it.

## 3. Insert a slide from a layout

`std::vector<SlideLayout> Document::slide_layouts()` lists the layouts: a
name, and per text placeholder its kind (`title`, `subtitle`, `body`) and its
position, which is where `insert_slide` puts it. A picker or the page can draw
a layout from that. `Slide Document::insert_slide(std::uint32_t index,
std::uint32_t layout)` adds an empty slide that holds the layout's text
placeholders, each with one empty paragraph for the caret.

- pptx: the layouts of every master, in `p:sldLayoutIdLst` order, named by
  `p:cSld/@name`. A second master often repeats the first one's names, so a
  repeat is numbered: "Title Slide (2)". A slide is a new part with a
  relationship to its layout, plus an entry in `p:sldIdLst`, the
  presentation's relationships and the content types. Each title, subtitle,
  body or content placeholder is copied as a `p:sp` with its `p:ph` and an
  empty `p:spPr`, as PowerPoint writes it: position and text style inherit
  (1b). Graphic placeholders, the date, footer and number, and a placeholder
  that neither the layout nor the master positions are left out.
- odp: the file's `style:presentation-page-layout`s, named by
  `style:display-name`, else by the built-in whose placeholders they hold
  (LibreOffice names its layouts `AL<n>T<m>`), else by `style:name`, a repeat
  numbered as in pptx; handout and notes layouts are left out. Then each
  built-in whose name is not taken. A built-in is written into `office:styles`
  the first time a slide uses it, scaled to the master's page; the list scales
  it to the first master's. The new `draw:page` takes the master of the slide
  before it, and each text placeholder becomes a `draw:frame` with
  `presentation:class`, the master's `<master>-title`, `-subtitle` or
  `-outline1` style where it has one, and a `draw:text-box`.

## 4. Insert a text box

`Frame Document::insert_text_box(const Element &page, const Measure &x,
const Measure &y, const Measure &width, const Measure &height)` on a slide
or a drawing page. Every length needs a fixed unit (`cm`, `mm`, `in`, `pt`,
`pc`, `px`) and the size must be positive. The frame draws no fill and no
outline and holds one empty paragraph.

- odf: `draw:frame` with a `draw:text-box`, ahead of `presentation:notes`.
  Its graphic style `odrTextBox<n>` (no stroke, no fill, growing with its
  text) is made once per document: LibreOffice's default graphic style would
  paint the frame.
- pptx: `p:sp` with `p:cNvSpPr txBox="1"`, a rectangle with `a:noFill`, an
  `a:spAutoFit` text body, and the next free `p:cNvPr/@id` on the slide. Its
  text follows `p:defaultTextStyle`, which a new pptx states as PowerPoint
  does: the theme's body font at 18pt.

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
