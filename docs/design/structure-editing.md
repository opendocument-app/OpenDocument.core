# Structure editing design

New odp and pptx files, and edits that add a sheet, a slide from a layout
or a text box, and remove or move a sheet or a slide. Shared editing rules
are in [editing](editing.md); creation rules are in
[document creation](document-creation.md).

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

`std::vector<Slide> Document::slide_layouts()` lists the layouts, each as the
empty slide `Slide Document::insert_slide(std::uint32_t index, std::uint32_t
layout)` makes from it: its text placeholders, each with one empty paragraph
for the caret. A layout is not under the root, so it is no slide of the
document, and `Document` refuses an edit to it. The engine builds the layouts
when it opens the document, and an insert copies one, so a layout and the slide
made from it are the same by construction.

- pptx: the layouts of every master, in `p:sldLayoutIdLst` order, named by
  `p:cSld/@name`. A second master often repeats the first one's names, so a
  repeat is numbered: "Title Slide (2)". A slide is a new part with a
  relationship to its layout, plus an entry in `p:sldIdLst`, the
  presentation's relationships and the content types. A layout is a detached
  `p:sld` holding each title, subtitle, body or content placeholder of the
  layout part as a `p:sp` with its `p:ph` and an empty `p:spPr`, as PowerPoint
  writes it: position and text style inherit (1b). The layout part's own text,
  such as "Click to edit Master title style", is not copied. Graphic
  placeholders, the date, footer and number, and a placeholder that neither
  the layout nor the master positions are left out.
- odp: the file's `style:presentation-page-layout`s, named by
  `style:display-name`, else by the built-in whose placeholders they hold
  (LibreOffice names its layouts `AL<n>T<m>`), else by `style:name`, a repeat
  numbered as in pptx; handout and notes layouts are left out. Then each
  built-in whose name is not taken. The list is made once, so writing a
  built-in moves no index. A layout is a detached `draw:page` on the first
  master. A built-in is written into `office:styles` the first time a slide
  uses it, scaled to that slide's master's page. The new `draw:page` takes
  the master of the slide before it, and each text placeholder becomes a
  `draw:frame` with `presentation:class`, the master's `<master>-title`,
  `-subtitle` or `-outline1` style where it has one, and a `draw:text-box`.

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

## 5. Ops

| op | fields |
|---|---|
| `insertSheet` | `index`, `name` |
| `insertSlide` | `index`, `layout`, `id`, `frames`, `paragraphs` |
| `insertTextBox` | `page`, `x`, `y`, `width`, `height`, `id`, `paragraph` |
| `removeSheet` | `sheet` |
| `moveSheet` | `sheet`, `index` |
| `removeSlide` | `page` |
| `moveSlide` | `page`, `index` |

`id` is the negative id of the created slide or frame. `frames` lists one
negative id per placeholder of the new slide, in document order, and
`paragraphs` one per placeholder's empty paragraph, so later ops can type into
them. Both are optional. A list whose length is not the layout's placeholder
count throws before the slide is made, because the page drew a different slide
than replay would make. `name` may be left out for the next free `SheetN`. The
lengths of `insertTextBox` are strings as `Measure` spells them, such as
`"2.5cm"`. A later op types into a placeholder by its paragraph id:

```json
{"op": "insertText", "parent": -4, "text": "Agenda", "id": -6}
```

Sheets keep their positional address, so an op after an `insertSheet`,
`removeSheet` or `moveSheet` counts the sheets as that op left them. `page`
names a slide by its id, a negative one for a slide inserted earlier in the
envelope. `index` of a move counts the places after it (section 8).

## 6. The page

The page applies the edit itself ([editing](editing.md) decision 1), so an
editable presentation or drawing page has to know what a new slide or text
box looks like.

- After the pages, the document view carries a `<template>` per layout
  (`odr-slide-template`, with `data-odr-layout` and `data-odr-name`) and one
  for a text box (`odr-text-box-template`). Each is the renderer's output for
  a layout of `slide_layouts()`, or for the text box an insert copies, so a
  template is what replay makes without a second model in the script.
- A layout's ids name nothing in the page. `data-odr-frames` and
  `data-odr-paragraphs` (`data-odr-frame` and `data-odr-paragraph` for the
  text box) say which elements take a fresh negative id; every other address
  in the clone is dropped.
- Each slide and drawing page carries its id as `data-odr-page`, not as
  `data-odr-id`: a range would take an addressed element that holds no run
  for something it can remove.
- A template shares the view's background classes and states its own rule,
  because the slide that stated the rule first may never be drawn.

| Script | What it does |
|---|---|
| `odr.editing.slideLayouts()` | the template names, in the order `insertSlide` counts them |
| `odr.editing.insertSlide({index, layout})` | clones a template before the `index`-th slide; without `index`, after the slide holding the selection, else last |
| `odr.editing.insertTextBox({page, x, y, width, height})` | clones the text box onto the `page`-th page, or the selected one, and sets its box; lengths are CSS lengths in a fixed unit |
| `odr.editing.removeSlide({page})` | removes the `page`-th slide, or the selected one |
| `odr.editing.moveSlide({page, index})` | puts the `page`-th slide, or the selected one, at `index`, counted after the move |

Both inserts move the caret into the first new paragraph, and typing there is
the existing `insertText` with `parent`. A removal moves it into the slide that
takes the place. A removal or a move records the slide by its
`data-odr-page`, and a move to where the slide is records nothing. One call
is one undo step. Under scope `paragraph` every gesture refuses with
`outOfScope`; a bad index, layout, page or length refuses with
`unsupportedEdit`. A view of one slide carries no template, and the gestures
refuse there; so do a removal and a move on a drawing, which carries none.

Not covered: a sheet. A spreadsheet view shows one sheet, so a host adds one
through `Document::insert_sheet` or the envelope op and renders again.

Known gaps:

- An odp layout sits on the first master. In an odp with several masters, a
  slide inserted after one on another master shows the first master until
  the next render. The saved file is right.
- A deck with hundreds of layouts carries hundreds of templates: about 2 KB
  each, more where a layout or master shows a picture.

## 7. Bindings

python, java and swift mirror the C++ calls and return element handles, a
layout among them. npm has no element surface, so `slideLayouts` answers
`[{name, placeholders: [{x, y, width, height}]}]`, the lengths as `Measure`
strings, `insertSlide` answers `{id, frames, paragraphs}` and `insertTextBox`
`{id, paragraph}`: the ids the ops of section 5 name, ready for
`appendText`. An npm session caches its views, so an inserted
sheet or slide, or an `edit`, drops them and the next `listViews` translates
again.

Removing and moving (section 8): python, java and swift take a sheet or a
slide through their existing `remove`, and add `move_sheet` / `moveSheet` /
`moveSheet(_:to:)` and the same for slides. npm has no sheet ids, so it takes
`removeSheet(sheet)` and `moveSheet(sheet, index)`, which replay the ops, a slide
by id through `removeElement` and `moveSlide(slideId, index)`; each drops the
cached views as an insert does.

## 8. Remove and move a sheet or a slide

`Document::remove` takes a sheet or a slide too. `void
Document::move_sheet(const Sheet &sheet, std::uint32_t index)` and `void
Document::move_slide(const Slide &slide, std::uint32_t index)` put it at
`index` among the others, counted after the move, so `index` is where it
ends up. A removed sheet or slide keeps its id and stops being reachable,
like every removed element.

Sheets:

- A workbook keeps one sheet: removing the last throws
  `std::invalid_argument`, as Excel and LibreOffice refuse it.
- A reference naming a removed sheet becomes `#REF!`, as both write it. The
  rewrite is the one a row delete runs, deleting every row and then every
  column of that sheet, so every place a row delete rewrites follows.
- A move rewrites no formula: formulas name sheets, and a 3D range
  (`Sheet1:Sheet3!A1`) spans whatever lies between its ends afterwards, as
  in Excel.
- Gap: removing the sheet at either end of a 3D range leaves the range
  naming it. Excel moves that end to the neighbouring sheet inside the range.
- xlsx: the `sheet` entry, the workbook relationship, the content type and
  the part go. A name local to the removed sheet goes; `localSheetId`,
  `activeTab` and `firstSheet` follow the positions. The parts the removed
  sheet alone refers to, such as its drawing, stay as unreferenced parts.
  A sheet holding a table or a pivot table, or one a pivot cache reads, is
  refused with `UnsupportedOperation`.
- ods: the `table:table` moves or goes.
- Every formula recomputes on the next `recalculate`.

Slides:

- odp: the `draw:page` moves or goes.
- pptx: `p:sldIdLst` is reordered. A removed slide also loses its
  presentation relationship, its content type, its part and its notes
  slide, and its entries in custom shows and sections. A moved slide joins
  the section of the slide it now follows, the first section at index 0,
  because PowerPoint repairs sections out of slide order.
- Gap: a relationship from another part to a removed slide stays, such as a
  slide-jump hyperlink or an outline entry in `viewProps.xml`. Its target is
  gone, which PowerPoint may offer to repair.

## Open work

Sheets:

- Rename a sheet, rewriting every formula, name and chart range that names
  it. The removal rewrite in section 8 already finds those references.
- Duplicate and hide a sheet.
- A page gesture for sheets. A sheet view shows one sheet, so it needs the
  view to show more than one, or a host-side tab bar that calls the ops.
- 3D ranges: removing the sheet at either end of a range should move that
  end inward, as Excel does (gap in section 8).
- xlsx: remove a sheet holding a table or a pivot table, or one a pivot
  cache reads; drop the parts only a removed sheet refers to, such as its
  drawing.

Slides:

- Move and resize a text box (section 4).
- Remove a shape, and insert a picture or a table. pptx editing covers runs,
  paragraphs and text style only (pptx `AGENTS.md`, open work 4).
- pptx: drop other parts' relationships to a removed slide, such as
  slide-jump hyperlinks and the outline in `viewProps.xml` (gap in section 8).
- odp with several masters: a slide inserted after one on another master
  shows the first master until the next render (gap in section 6).
- A deck with hundreds of layouts carries hundreds of templates (gap in
  section 6). Templates could load on demand.

Both:

- Open the edited xlsx and pptx files in Excel and PowerPoint, and run
  `OpenXmlValidator` on them (see Validation).

## Validation

`DocumentCreate.*` and the per-feature gtests, then a LibreOffice round trip
(`--convert-to pdf`) of every new and edited package. Keynote opens pptx as a
second reader. PowerPoint and `OpenXmlValidator` are not on this machine.
