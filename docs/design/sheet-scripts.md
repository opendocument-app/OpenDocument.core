# Sheet scripts design

Status: built.

Related: [`single-page-layout.md`](single-page-layout.md) shows one sheet of
a workbook at a time; [`sheet-grid.md`](sheet-grid.md) draws and edits the
grid past each sheet's content; [`spreadsheet-editing.md`](spreadsheet-editing.md)
holds the sheet editor.

## Problem

A workbook's whole-document view holds every sheet in one page. The two
sheet scripts bound to the first `.odr-sheet` they found: the second sheet
had no sticky headers, no row loading, no grid and no editor. With the
single-page layout a reader turns to that sheet, so it has to work.

## Decisions

### 1. Each script binds every sheet

`spreadsheet.js` and `sheet-editing.js` are a small coordinator and, after
it, the code they always were, now a function of one sheet. The coordinator
runs that function for every `.odr-sheet` of the page. A view of one sheet
gets one, as before.

- The body is not re-indented: it is the argument the coordinator is called
  with, so a diff shows what changed and not every line.
- Each sheet keeps all its own state: the pin, the selection, the row
  index, the grid, the editor overlay, the formula cells and what they read.
- A sheet's column wash is scoped to it,
  `.odr-sheet[data-odr-sheet="N"] tr>:nth-child(k)`, so hovering a column on
  one sheet washes no other.

### 2. `odr.sheet` answers for the sheet the reader is in

`odr.sheet` is a getter: the sheet pinned last, or under a single-page
layout the sheet shown (`viewport.js` dispatches `odr-page-change` on the
document for the page's own scripts). `odr.sheets` lists every sheet's
interface, and `odr.goToCell` goes to a cell of the active sheet.

- **One pin on the page.** A sheet that takes a pin clears every other
  sheet's, by a click or by the script, so a key reaches one sheet only.
- The editor of a sheet works on its own sheet's interface
  (`odr.sheetFor(table)`), never on `odr.sheet`, so a write cannot land on
  another sheet because the reader turned the page meanwhile.

### 3. One history for every sheet

`editing.js` takes one editor from `sheet-editing.js`, whatever the number
of sheets. Its steps are one history, in the order taken.

- **The log replays in that order.** An op on one sheet can move what a
  formula on another names: inserting rows on `two` rewrites
  `=two!A5` on `one`. Replaying the sheets one after the other would move a
  reference typed after the insert as well.
- **Undo and redo take the last step, on whichever sheet** it was, and
  repaint that sheet.
- **A save** lets every sheet read the log it saved, then empties it, then
  repaints every sheet's stale marks.
- A sheet's formula bookkeeping reads its own steps only, as a view of one
  sheet did: which formula cells a write makes stale, the moves the formula
  module replays. What one sheet's edit does to a formula on another is
  computed by the save, as before.

### 4. What goes to which sheet

| Call | Sheet |
|---|---|
| `format`, `toggle`, `insertRows`, `deleteRows`, `insertColumns`, `deleteColumns` | the active one |
| `lockAt`, `refuseAt`, `editAt`, `getSheetOptions` | the active one |
| `setSheetOptions`, `enable`, `disable` | every sheet |
| `loadFormulas` | every sheet, one module for all of them |

## Left

- **Formulas across sheets in the page.** A write on one sheet does not
  mark the formulas of another that read it stale; the save computes them.
