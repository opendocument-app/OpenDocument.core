# Sheet grid design

Status: proposed, not built.

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

`.odr-sheet` carries what the page needs to draw past the content:

| Attribute | What it states |
|---|---|
| `data-odr-sheet-max` | the last row and column the format allows, as `rows,columns` |
| `data-odr-sheet-default-size` | the width of an empty column and the height of an empty row, in px |
| `data-odr-sheet-cut` | present where `spreadsheet_limit` or the cell budget cut the sheet (`HtmlSheetCut`): `rows`, `columns` or both |

- **The maximum** is the format's: 1,048,576 × 16,384 for xlsx, ods and csv,
  65,536 × 256 for xls. An ods states its grid as its dimensions, so its own
  dimensions win where they are smaller; an xlsx states only its used range,
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
without its script stay as they are.

### 2. The page draws the grid

`spreadsheet.js` paints the region below and to the right of the content.

- **Gridlines** are CSS: a background repeated at the default column width
  and row height on two areas, one below the last row and one right of the
  last column, so no element exists per cell. They follow
  `HtmlConfig::spreadsheet_gridlines`: none, soft or hard.
- **The ruler** goes on: column letters past the last column, row numbers
  past the last row. Only the labels in view are elements, reused as the
  reader scrolls, so the DOM is a screen of labels whatever the position.
  They stick as the ruler does now.
- **The area grows as the reader goes.** Its size starts at a screen past
  the content and grows by a screen when the reader comes near its edge, up
  to the maximum, as Excel's scroll bar does. A jump grows it to the target
  at once: a script call to go to a cell (`odr.goToCell(column, row)`, by
  index), and the keys that jump in a desktop application, Ctrl+End and
  Ctrl+arrow, which the page does not take today. The maximum height is
  never laid out as one element; past the browser's limit, the area holds the
  window around the target and stands in for the rows above it with a
  spacer, so the scroll position stays meaningful.
- **Selecting** an empty cell works by coordinate. The pinned cell, the one
  a click or an arrow key selects (`odr.sheet.pinned()`), moves past the
  content into the grid: a click is turned into a row and a column from the
  default size, and the arrows step on past the last row and column. Its
  mark is drawn over the grid at that position, and `pinned()` reports the
  position as it does for a cell that exists.

The page grid is on for every sheet view. `odr.editing.setSheetOptions`
takes `grid: false` to drop it, and `HtmlConfig::spreadsheet_grid` (on by
default) for a host that wants the old canvas from the first frame.

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

- **A workbook's whole-document view drives one sheet.** The sheet scripts
  bind to the first `.odr-sheet` of the page (`single-page-layout.md`,
  "Left"). The grid lands in the views of one sheet first.
- **Frozen panes** are not rendered today. Where they come, the grid starts
  past them.

## Open questions

- Whether a jump past the browser's limit is worth the spacer, or whether
  the area stops at the limit and a jump past it is refused.
- Whether the page grid follows `color_scheme`'s dark sheet from the start:
  the canvas and the gridline colours are already variables there.
