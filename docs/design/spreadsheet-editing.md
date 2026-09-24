# Spreadsheet editing design

Status: cells edit and save in `.ods` and `.xlsx`, formulas parse and track
their dependents, and nothing evaluates a formula yet.

Related: [`editing.md`](editing.md) holds the mode frame every editor shares,
[`document-editing.md`](document-editing.md) the document view and
[`txt-editing.md`](txt-editing.md) the plain-text view.
[`odf/AGENTS.md`](../../src/odr/internal/odf/AGENTS.md) and
[`ooxml/spreadsheet/AGENTS.md`](../../src/odr/internal/ooxml/spreadsheet/AGENTS.md)
describe the read side.

## Problem

A sheet adds two format complications over a text document, and one thing
text never had.

- **ODS collapses repeats.** One `<table:table-cell
  table:number-columns-repeated="1000"/>` stands for a thousand cells, a row
  repeats the same way, and an empty cell has no element at all. The cell a
  user types into may not exist as a node.
- **XLSX shares strings.** A `t="s"` cell reads its text from
  `sharedStrings.xml`, so a write into that text edits every cell that shares
  it.
- **Formulas.** A cached result goes stale the moment an input changes.

## Where the code is

| Piece | Where |
|---|---|
| Cell value | `SheetCell::value()` returns a `CellValue`: type, number, text and formula. Abstract hook `sheet_cell_value`; `sheet_cell_value_type` stays the cheap question the renderer asks |
| ODS write | `odf_document.cpp::sheet_set_cell`. `claim_cell` cuts a repeated run and states the `text:p` an empty cell lacks, `grow_to_cell` and `grow_columns` append past the sheet, `reindex_sheet` rebuilds the cell index off the dom |
| ODS save | `odf_document.cpp::save` re-serialises `content.xml` and byte-copies the rest |
| XLSX write | `ooxml_spreadsheet_document.cpp::sheet_set_cell`. `insert_cell` states a `<c>` the file lacks. A string goes inline as `t="inlineStr"`, never back into `sharedStrings.xml` |
| XLSX save | `ooxml_spreadsheet_document.cpp::save` writes the worksheets and `workbook.xml`, copies the rest, and sets `calcPr/@fullCalcOnLoad="1"` |
| Formulas | `internal/formula/` parses OpenFormula and OOXML into one AST, writes it back, and shifts it for an ooxml shared formula |
| Dependencies | `internal::SheetDependencies`, built once off the decoded document. `Document::dependents` and `Document::unresolved_formulas` expose it |
| Stale results | An odf write drops the cached result of every dependent (`drop_stale_results`). An ooxml write keeps them, because every save sets `fullCalcOnLoad` |
| Browser: sheet script | `html/frontend/spreadsheet.js` owns pin, raise, sort and the position map, and publishes `odr.sheet` |
| Browser: the mode | `html/frontend/editing.js` owns `odr.editing`, the refusals, the log and the `odr.on*` callbacks |
| Browser: sheet editor | `html/frontend/sheet-editing.js` attaches the cell overlay and the stale marks to the mode |
| Wire format | `document.cpp::Document::edit` dispatches the op envelope |
| Capabilities | `file_type_table.cpp`: `ods` and `xlsx` declare `edit` and `save`, `csv` declares neither |
| Tests | `test/src/document_edit_test.cpp`, `test/src/sheet_dependencies_test.cpp`, `test/browser/sheet/` |

## Decisions

### 1. A cell is the unit of editing, addressed by position

The op is `setCell {sheet, column, row, value}`, not an element id and not a
path.

**Why:** an empty cell, and a repeated ODS cell, has no element, so an id
cannot name it. A position can, and it is what the file itself uses. Nothing
is renumbered when the only op replaces a cell's whole content, so the
id-stability question of `editing.md` does not arise for a sheet. Ids stay the
answer for text, where an insertion point inside a paragraph has no position.

Coalescing is a map keyed by position, last write wins. Undo keeps the previous
value beside the op. The log is idempotent.

### 2. One op-log envelope

```json
{
  "version": 2,
  "ops": [
    {"op": "setCell", "sheet": 0, "column": 1, "row": 2,
     "value": {"type": "number", "number": 12.5, "text": "12.5"}},
    {"op": "setCell", "sheet": 0, "column": 1, "row": 3,
     "value": {"type": "string", "text": "total"}},
    {"op": "setCell", "sheet": 0, "column": 1, "row": 4,
     "value": {"type": "empty"}}
  ]
}
```

`Document::edit` throws on the first op it cannot apply and leaves the ones
before it applied. A host replays onto a fresh decode, so a failed replay
costs nothing. `version` is the wire version. A document stamp (`editing.md`
decision 7) is not needed for a sheet: a position is meaningful against any
decode of the same file.

### 3. Editing is a browser mode, not markup

`odr.editing.enable()` and `disable()` switch the mode without a second
translate. The page carries only what the browser cannot work out itself:
`data-odr-lock` on a cell that refuses a write, with its reason (`formula`,
`rich` for several paragraphs, a link or a line break, `shapes` where the
cell is nothing but its anchored drawings), and `data-odr-editable` on
`<body>`. Every other cell is editable, an empty one included.

The editor is an overlay the script places over the cell, so the sheet's DOM
stays untouched until the commit patches the cell.

**Why:** a `td` holds wrappers and shapes, so a static `contenteditable` is
the wrong tool. Refusal is an event, not a silent no-op: a click on a locked
cell outlines it and calls `odr.onEditRefused` (decision 7).

The mode itself is generic and lives in `editing.js` (`editing.md` decisions
9 to 12). What stays here is the sheet's own: the overlay, the locks, the
position map and the `setCell` op.

### 4. The type follows the content

A strict grammar parses the typed string: optional sign, digits, one `.`,
optional exponent is a number. Anything else is a string. A leading `'` forces
a string. `=` is refused with `formulaInput` until the evaluator exists.

**Why:** the file states the type per cell (`office:value-type`, `c/@t`) and
changes it freely, so a cell has no fixed type. A number cell writes
`office:value` and the `text:p`, or `<v>` alone. A string cell drops
`office:value`, or becomes `t="inlineStr"` with `<is><t>`.

Number formats are not parsed, so a formatted cell edited to `2000` shows
`2000` until a producer reopens the file. Only `.` is a decimal separator,
because no locale is read anywhere.

### 5. Formulas are recomputed in C++, once, and reached through the host

The evaluator, when it exists, runs in C++ only. The browser hands the host
the op log per commit, and gets back the cells whose display changed.

**Why not JavaScript generated from the tree:** two function libraries that
drift. **Why not the engine as wasm inside the HTML:** every current host has
the engine in process. `HtmlConfig::embed_shipped_resources` is where such a
blob would go if a host without a bridge appears.

Until then the file has to stay honest. `.xlsx` has a switch for this,
`calcPr/@fullCalcOnLoad="1"` (ECMA-376 18.2.2), set on every save. `.ods` has
none, so an odf write removes the cached result of every dependent cell: the
value attributes and the `text:p`. A cell stating a formula and no result is
one a reader has to compute, and such a cell renders empty here.

### 6. The page stays up until the user saves

The host takes the log from `odr.editing.getOperations()`, never re-translates
for a save, and calls `odr.editing.committed()` after a successful save so the
log resets. Each sheet view runs its own script, so a host showing sheets as
separate pages collects a log per view. Every op carries its sheet, so
concatenation is the merge.

### 7. Host events are flat `odr.on*` callbacks, and the host owns the wording

```js
// {sheet, column, row, reason, code, message}
odr.onEditRefused = function (event) {};
// {dirty, operations, canUndo, canRedo}
odr.onEditChange = function (event) {};
// {editing, editable, reason, code, message}
odr.onEditModeChange = function (event) {};
// {sheet, cells} - the formula cells an edit left computing an old input
odr.onCellsStale = function (event) {};
```

- The `code` is an `odr::ErrorCode` (`src/odr/error_code.hpp`), written into
  the page by `html/frontend.cpp::write_error_codes`. Exceptions sit below
  1000, refusals from 1001. Codes are appended, never renumbered;
  `error_code_test.cpp` and `wasm/tests/enums.test.mjs` pin them.
- `reason` is the same code spelled for a reader of the log. `message` is
  English and for the console; a host maps `code` to its own wording.
- Every callback takes one object, so a field can be added without breaking a
  host.
- The page drops an identical refusal repeated within a couple of seconds.
- `onEditChange` fires on every commit, undo, redo and `committed()`; `dirty`
  drives a save button and a back-press warning.

Attaching: droid and ios assign the callbacks once the WebView has loaded the
page. A browser host assigns on the iframe's `contentWindow.odr` at its
`load`; a cross-origin sandboxed frame is out of scope.

**Why not `odr.onError`:** a refusal is expected UX, frequent, and carries a
position. Sharing the code table keeps one lookup for both.

### 8. The sheet script owns the position map, and publishes it as `odr.sheet`

```js
odr.sheet.cellAt(column, row); // the `td`, null past the sheet's extent
odr.sheet.positionOf(cell);    // {column, row}, null for a header
odr.sheet.pinned();            // {column, row, cell}, null for none
odr.sheet.pin(position);       // null clears; false where there is no cell
odr.sheet.lower();             // puts back a cell the pin raised
odr.sheet.valueAt(column, row);       // what the page shows, as an op states it
odr.sheet.formulaAt(column, row);     // the expression a formula cell states
odr.sheet.showValue(column, row, v);  // shows it, and reflows the row
odr.sheet.reflow(row);                // the spill geometry, measured again
```

**Why:** the map is not a walk over `colspan`. Sorting moves the `<tr>`s, so a
row is named by its `<th>` label, and a `rowspan` leaves positions unwritten.
The script that reorders rows has to own the map, and one owner of the pin
classes and the raise wrapper avoids an overlay open over a cell the other
script lowers. The read-only view does not carry the editor, so the two stay
separate scripts. The coordinates are the ones an op names, never a DOM index.

## Formulas, read side

- `internal/formula` parses `of:=SUM([.A1:.B2])` (`table:formula`) and
  `SUM(A1:B2)` (`<f>`) with one recursive descent that branches on a
  `Syntax`. A named expression, a reference over several sheets and a
  spelling past the grid stay opaque names. A formula that does not parse
  answers nothing.
- The writer and `shift` make an ooxml shared formula readable: a member reads
  the master's expression moved by the offset between the two cells.
- `SheetAdapter::sheet_visit_formulas` hands out the cells the file spells,
  not the positions they cover, so a repeated row is a handful of nodes.
- A formula the graph can read no position out of is in
  `Document::unresolved_formulas` and keeps its cached result.
- The view writes `data-odr-formula` and `data-odr-reads` on a formula cell,
  as editing scaffolding only. A commit marks the dependents `odr-sheet-stale`
  off the coalesced log (`repaintStale`) and raises `odr.onCellsStale`, so an
  undo takes its marks back.

## Open work

- The evaluator: a typed value, error propagation, the frequent functions,
  incremental recompute in topological order with cycles reported, and
  `Document::recalculate(operations)` returning the changed cells. Formula
  input in the editor comes with it.
- Number formats (`number:number-style`, `numFmt`), dates and booleans as
  their own kinds. This also fixes `.xlsx` serials on the read side.
- Multi-line cells, cell style edits, insert and delete of rows and columns.
- `.csv` save.
- Sheets past `spreadsheet_limit` or `spreadsheet_cell_limit` are not in the
  page; the mode should say so where a view reports a `sheet_cut`.
- The decimal separator and the document locale are read nowhere.
- A written string cell that looks numeric becomes a number; `'` is the escape.
