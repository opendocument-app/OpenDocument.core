# Sheet grid design

Status: step 1 built (decisions 1 and 2); editing past the content (decision
3) is not.

Related: [`lazy-loading.md`](lazy-loading.md) holds the row chunks a sheet
is written in. [`spreadsheet-editing.md`](spreadsheet-editing.md) holds the
sheet editor, which the second step extends past the content.

## Problem

A sheet view stops at the last row and column that hold something
(`HtmlConfig::spreadsheet_limit_by_content`). Past them, `spreadsheet.css`
paints plain canvas. Excel and LibreOffice keep the grid going to the last
row and column of the format, so a reader sees where the sheet goes on, and
can select and type past what is there.

That cannot be translated. An xlsx has 1,048,576 rows by 16,384 columns, about
17 billion cells. No page holds them as `<td>`s, and no browser lays out an
element that tall: a million rows of 20 px are 21 million px, past Firefox's
limit of about 17.9 million and near Chrome's of about 33.5 million. The
desktop applications do not hold those cells either. They draw the window in
view and compute the rest, and their scroll bar grows as the reader goes.

The empty region is also not content. It has nothing to translate. So the
renderer states a few facts, and the page draws the rest.

## Decisions

### 1. The renderer states facts, not cells

`.odr-sheet` carries what the page needs to draw past the content, each as
`columns,rows` like the `data-odr-cut` the editor already reads:

| Attribute | What it states |
|---|---|
| `data-odr-sheet-extent` | the rendered content, which a row chunk that has not loaded does not show the page |
| `data-odr-sheet-max` | the last column and row the format allows |
| `data-odr-sheet-default-size` | the width of an empty column and the height of an empty row, in px |
| `data-odr-cut` | the whole content where `spreadsheet_limit` or the cell budget cut the sheet (`HtmlSheetCut`); written for the grid too now, not only for an editable render |

- **The maximum** is the format's: 1,048,576 × 16,384 for xlsx, ods and csv,
  65,536 × 256 for xls, 1,000,000 × 1,000 for Numbers. An ods states its grid
  as its dimensions, so its own dimensions win where they are smaller; an xlsx states only its used range,
  so its dimensions say nothing about the maximum.
- **The default size** is what an empty column and row take in the format:
  the sheet's default column width and row height where the file states
  them, else the application's (Excel: 8.43 characters by 15 pt; LibreOffice:
  2.26 cm by 0.45 cm). Reading the file's defaults is new model work; until
  then the renderer states the application's.
- **No grid past a cut.** Where the render is cut, the area past the
  rendered cells holds content the page does not show. Drawing empty cells
  there would claim it is empty. The page draws nothing past a cut edge, as
  now.

No `<td>` is written for an empty cell, so the markup, a print and a page
without its script stay as they are. `HtmlConfig::spreadsheet_grid` (on by
default) drops the three new attributes, and with them the grid.

### 2. The page draws the grid

`spreadsheet.js` paints the region below and to the right of the content.

- **Gridlines** are CSS: a background repeated at the default column width
  and row height on two areas, one below the last row and one right of the
  last column, so no element exists per cell. They follow
  `HtmlConfig::spreadsheet_gridlines` (none, soft or hard) and the dark
  sheet's colours.
- **The ruler** goes on: column letters past the last column, row numbers
  past the last row. Only the labels in view are elements, reused as the
  reader scrolls, so the DOM is a screen of labels whatever the position.
  They stick as the ruler does now.
- **The area grows as the reader goes.** It keeps two screens past the view
  and grows as the reader scrolls, up to the maximum, as Excel's scroll bar does. A jump grows it to the target
  at once: `odr.goToCell(column, row)` (also `odr.sheet.goToCell`, by
  index) pins the cell, empty or not, and scrolls it into view. It answers
  false past the maximum or a cut edge.
- **The height stops at the browser's limit.** The area below the content
  stays under 15 million px, short of Firefox's 17.9 million; a jump past
  that is refused. At Excel's 20 px a row, that is some 750,000 rows.
- **Selecting** an empty cell works by coordinate. The pinned cell, the one
  a click or an arrow key selects (`odr.sheet.pinned()`), moves past the
  content into the grid: a click is turned into a row and a column from the
  default size, and the editor's arrows step on past the last row and
  column. Its mark is drawn over the grid at that position, and `pinned()`
  reports the position with a null `cell`.
- **Not in print.** The grid is screen only.

The page grid is on for every sheet view that states its facts.
`odr.sheet.setGrid(false)` drops it in any page (`odr.sheet.hasGrid()`
answers whether it is drawn), and
`odr.editing.setSheetOptions({grid: false})` in an editable one;
`HtmlConfig::spreadsheet_grid` keeps the facts out of the markup for a host
that wants the old canvas from the first frame.

### 3. Editing past the content is a second step

The editor edits `<td>`s: it reads a cell's text, writes into it and keeps
its undo by element. Typing into an empty cell past the content needs a cell
to type into.

- The page makes the row and the cell when an edit starts there: a `<tr>`
  in a table of its own after the last chunk, holding cells up to the
  edited column, sized as the grid draws them. The rows between the content
  and it stay drawn by the grid, not made.
- The edit is the `setCell` it is inside the content, at that position.
  Whether every engine takes a write past its content (an ods row inside
  `table:number-rows-repeated`, an xlsx row with no `<row>`) is checked
  before this step is built.
- Inserting and deleting rows and columns past the content is nothing to
  record: the grid there is empty.

The first step ships without this: the grid past the content is drawn,
scrolled and selected, and an edit there is refused with `unsupportedEdit`.

## Left

- **Jump keys.** Ctrl+End and Ctrl+arrow, which jump in a desktop
  application, are not taken yet; `odr.goToCell` is what they would call.
- **Past the browser's height limit**, a spacer could stand in for the rows
  above a far target, so a jump to row 1,048,576 works. Not built: the area
  stops at the limit.
- **The file's own default column width and row height** are not read yet;
  the application's stand in (decision 1).
- **A workbook's whole-document view drives one sheet.** The sheet scripts
  bind to the first `.odr-sheet` of the page (`single-page-layout.md`,
  "Left"). The grid lands in the views of one sheet first.
- **Frozen panes** are not rendered today. Where they come, the grid starts
  past them.
