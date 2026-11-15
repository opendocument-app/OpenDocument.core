# Structure editing design

New odp and pptx files, and edits that add, remove or move a sheet or a
slide, or add a text box. Shared editing rules are in [editing](editing.md);
creation rules are in [document creation](document-creation.md).

## Landed

- **New presentations:** one "Title Slide" slide on a 28 cm × 15.75 cm page
  (LibreOffice's 16:9). Six layouts: Title Slide, Title and Content, Section
  Header, Two Content, Title Only, Blank. One geometry table feeds both
  formats, so a layout sits at the same place in each.
- **Insert a sheet:** `Spreadsheet::insert_sheet(index, name)`. An empty name
  takes the first free `SheetN`.
- **Insert a slide:** `Presentation::slide_layouts()` and
  `Presentation::insert_slide(index, layout)`.
- **Insert a text box:** `insert_text_box(page, x, y, width, height)` on
  `Presentation` and `Drawing`, for a slide or a drawing page.
- **Remove and move:** `Document::remove` takes a sheet or a slide;
  `Spreadsheet::move_sheet(sheet, index)` and
  `Presentation::move_slide(slide, index)`.
- **Ops:** see the table below.
- **The page:** slide and text-box templates and five `odr.editing` gestures.
- **Bindings:** python, java, swift and npm.

## Rules

### Sheets

- A name another sheet has (ignoring ASCII case), one with `[]*?:/\`, a
  leading or trailing apostrophe, or more than 31 characters throws. 31 is
  Excel's limit, stricter than LibreOffice's.
- A workbook keeps one sheet: removing the last throws
  `std::invalid_argument`, as Excel and LibreOffice refuse it.
- Formulas name sheets, so an insert or a move rewrites none. A 3D range
  (`Sheet1:Sheet3!A1`) spans whatever lies between its ends, as in Excel.
- A reference to a removed sheet becomes `#REF!`. The rewrite is a row
  delete of every row and then every column of that sheet, so everything a
  row delete rewrites follows.
- After any sheet edit the dependency graph is dropped and every formula
  recomputes on the next `recalculate`.
- xlsx: sheet-index state (`definedName/@localSheetId`, `activeTab`,
  `firstSheet`) follows the positions. A removal also drops the sheet's
  relationship, content type and part, and its local names.

### Slides and layouts

- A layout is the empty slide `insert_slide` copies, built when the document
  opens and held outside the root, so `Document` refuses an edit to it. A
  layout and the slide made from it are the same by construction.
- Each text placeholder of a layout holds one empty paragraph for the caret.
  Graphic, date, footer and number placeholders are left out, and so is a
  placeholder nothing positions.
- pptx layouts are those of every master, in `p:sldLayoutIdLst` order. A
  repeated name is numbered: "Title Slide (2)". A placeholder is a `p:sp`
  with its `p:ph` and an empty `p:spPr`, as PowerPoint writes it, so its
  position and text style inherit from the layout and master.
- odp layouts are the file's `style:presentation-page-layout`s, named by
  `style:display-name`, else by the built-in whose placeholders they hold
  (LibreOffice names them `AL<n>T<m>`), followed by each built-in whose name
  is not taken. A new `draw:page` takes the master of the slide before it.
- A text box draws no fill and no outline. Its lengths need a fixed unit
  (`cm`, `mm`, `in`, `pt`, `pc`, `px`) and a positive size. odf gives it a
  graphic style `odrTextBox<n>`, because LibreOffice's default graphic style
  paints the frame. pptx text follows `p:defaultTextStyle`.
- pptx removal drops the slide's relationship, content type, part, notes
  slide, and its custom-show and section entries.
- A moved pptx slide joins the section of the slide it now follows, or the
  first section at index 0, because PowerPoint repairs sections that are out
  of slide order.

### Ops

| op | fields |
|---|---|
| `insertSheet` | `index`, `name` |
| `insertSlide` | `index`, `layout`, `id`, `frames`, `paragraphs` |
| `insertTextBox` | `page`, `x`, `y`, `width`, `height`, `id`, `paragraph` |
| `removeSheet` | `sheet` |
| `moveSheet` | `sheet`, `index` |
| `removeSlide` | `page` |
| `moveSlide` | `page`, `index` |

- `id`, `frames` and `paragraphs` are the negative ids of what the op
  creates, so later ops can type into it:
  `{"op": "insertText", "parent": -4, "text": "Agenda", "id": -6}`.
  `frames` and `paragraphs` are optional. A list whose length is not the
  layout's placeholder count throws before the slide is made: the page drew
  a different slide than replay would make.
- A sheet is addressed by position, counted as the ops before it left the
  sheets. `page` is a slide id, negative for one inserted earlier in the
  envelope.
- A move's `index` is where the element ends up, counted after the move.
- Lengths are `Measure` strings, such as `"2.5cm"`.

### The page

The page applies an edit itself ([editing](editing.md) decision 1), so an
editable presentation or drawing view carries what it needs to draw one.

- A `<template>` per layout (`odr-slide-template`) and one for a text box
  (`odr-text-box-template`), each rendered from what replay copies, so the
  script needs no second model. `data-odr-frames` and `data-odr-paragraphs`
  name the elements that take a fresh negative id; every other id in a
  clone is dropped.
- A slide carries its id as `data-odr-page`, not `data-odr-id`, so a range
  cannot select a slide and remove it.
- A template states its own background rule, because the slide that stated
  it first may never be drawn.

| Script | What it does |
|---|---|
| `odr.editing.slideLayouts()` | the template names, in `insertSlide` order |
| `odr.editing.insertSlide({index, layout})` | before the `index`-th slide; without `index`, after the selected one, else last |
| `odr.editing.insertTextBox({page, x, y, width, height})` | on the `page`-th page or the selected one; CSS lengths in a fixed unit |
| `odr.editing.removeSlide({page})` | the `page`-th slide or the selected one |
| `odr.editing.moveSlide({page, index})` | the `page`-th slide or the selected one, to `index` |

- One call is one undo step. A move to where the slide is records nothing.
- Under scope `paragraph` every gesture refuses with `outOfScope`; a bad
  index, layout, page or length with `unsupportedEdit`. A view of one slide
  and a drawing carry no templates, so the gestures refuse there.
- There is no sheet gesture: a sheet view shows one sheet, so a host edits
  sheets through the API or the ops and renders again.

### Bindings

python, java and swift mirror the C++ calls. npm has no element surface:

- `slideLayouts` answers `[{name, placeholders: [{x, y, width, height}]}]`.
- `insertSlide` answers `{id, frames, paragraphs}` and `insertTextBox`
  `{id, paragraph}`, ready for `appendText`.
- `removeSheet(sheet)` and `moveSheet(sheet, index)` take positions and
  replay the ops; a slide goes through `removeElement` and
  `moveSlide(slideId, index)`.
- Every structural edit drops the cached views, so the next `listViews`
  translates again.

## Open work

Sheets:

- Rename a sheet, rewriting every formula, name and chart range that names
  it. The removal rewrite already finds those references.
- Duplicate and hide a sheet.
- A page gesture for sheets: the view needs to show more than one sheet, or
  a host-side tab bar calls the ops.
- 3D ranges: removing the sheet at either end of a range leaves the range
  naming it. Excel moves that end inward.
- xlsx: removing a sheet holding a table or a pivot table, or one a pivot
  cache reads, is refused with `UnsupportedOperation`. The parts only a
  removed sheet refers to, such as its drawing, stay unreferenced.

Slides:

- Move and resize a text box.
- Remove a shape, and insert a picture or a table. pptx editing covers runs,
  paragraphs and text style only (pptx `AGENTS.md`, open work 4).
- pptx: other parts' relationships to a removed slide stay, such as
  slide-jump hyperlinks and the outline in `viewProps.xml`. PowerPoint may
  offer to repair them.
- odp with several masters: a slide inserted after one on another master
  shows the first master until the next render. The saved file is right.
- A deck with hundreds of layouts carries hundreds of templates, about 2 KB
  each. Templates could load on demand.

Validation:

- `DocumentCreate.*` and the per-feature gtests, then a LibreOffice round
  trip (`--convert-to pdf`) of every new and edited package. Keynote reads
  pptx as a second reader.
- Not yet done: opening the edited xlsx and pptx files in Excel and
  PowerPoint, and running `OpenXmlValidator` on them.
