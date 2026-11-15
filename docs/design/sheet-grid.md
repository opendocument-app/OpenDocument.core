# Sheet grid design

Status: built.

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

### 3. Editing past the content

The editor edits `<td>`s: it reads a cell's text, writes into it and keeps
its undo by element. Typing into an empty cell past the content needs a cell
to type into.

- **The page makes the columns and rows up to the cell** when an edit starts
  there (`odr.sheet.materialize`), as an insert makes them: a column with its
  letter and a `td` in every row, a row with a `td` in every column, at the
  grid's size. Then the editor opens as over any cell, and the grid starts
  past the new last row and column.
  - Not a cell of its own beside the content: the row index finds a row by
    its label, and every chunk table and the ruler share one set of columns,
    so a lone cell would stand on another row's label or leave rows without
    their gridlines.
  - Within a budget of 200,000 cells, because a jump far out would make a
    cell for every row and column on the way. Past it the edit is refused
    with `unsupportedEdit`.
- **The write owns what was made.** The commit is the `setCell` an edit
  inside the content makes; its undo takes the rows and columns away again,
  and its redo puts the same elements back. An edit that ends without a
  write (Escape, or nothing typed) takes them away at once.
- **Opening the editor there** works as on a cell: typing, Enter or F2 on the
  pinned empty cell, a double click, or a click where `editOnClick` holds.
- **Inserting and deleting rows and columns in the grid** records nothing
  and is taken: the grid there is empty. Past a `spreadsheet_limit` cut no
  grid is drawn, and such an edit moves what the file holds there.
- **Every engine takes the write.** A test writes below and beside the
  content of a new and a real ods (whose empty rows are one
  `table:table-row` repeated), a new and a real xlsx (no `<row>` past the
  content) and a csv, saves, reopens and reads it back; LibreOffice reads the
  saved ods and xlsx with the cells in place. The csv writer needed one fix:
  an empty row it adds is written with the sheet's fields (`,,`), not one
  `""`, so the file still reads as a csv.

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
- **Formatting past the content.** A style chord on an empty cell does
  nothing yet; it would make the cell as an edit does.
- **An edit past the budget**, far below a wide sheet, is refused. Making
  only the target row, with the rows on the way left to the grid, needs the
  row index to take rows out of order.
