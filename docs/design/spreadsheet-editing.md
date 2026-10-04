# Spreadsheet editing design

Status: cells edit and save in `.ods`, `.xlsx` and `.csv`, with their
styles, number formats, dates, and inserted and deleted rows and columns.
Formulas parse, track their dependents and evaluate. A recalculation and a
save write the results the evaluator computes.

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
| Number formats | `internal/number_format/` parses a format code and formats a number with it; `odf/odf_number_format.cpp` turns an ods data style into a format code |
| Evaluation | `formula::evaluate` computes a parsed formula, or gives no answer. `formula_value.hpp` holds the values and the `Settings`, `formula_function.hpp` the functions. `internal::SheetCellSource` reads the cells of a decoded document, and `abstract::Document::formula_settings` states how it computes. `internal::recalculate` (`sheet_recalculation.cpp`) computes the stale cells and writes them with `SheetAdapter::sheet_set_result`; `internal::Document` notes the edits. `test/src/formula_corpus_test.cpp` holds decision 29 |
| Dependencies | `internal::SheetDependencies`, built off the decoded document and again after a row edit. `Document::dependents` and `Document::unresolved_formulas` expose it |
| Row and column edits | `formula::SheetEdit` and `formula::move_references` move a reference along either axis, `formula::move_addresses` an address list. `odf_sheet_references.cpp` and `ooxml_spreadsheet_references.cpp` walk what each file addresses |
| Stale results | An odf write drops the cached result of every dependent (`drop_stale_results`). An ooxml write keeps them, because every save sets `fullCalcOnLoad`. `Document::recalculate`, and a save after an edit, write what the evaluator computes |
| Browser: sheet script | `html/frontend/spreadsheet.js` owns pin, raise, sort and the position map, and publishes `odr.sheet` |
| Browser: the mode | `html/frontend/editing.js` owns `odr.editing`, the refusals, the log and the `odr.on*` callbacks |
| Browser: sheet editor | `html/frontend/sheet-editing.js` attaches the cell overlay and the stale marks to the mode |
| Wire format | `document.cpp::Document::edit` dispatches the op envelope |
| CSV write and save | `csv_document.cpp`: `set_cell` grows the rows, `save` writes UTF-8 with the line end and the `sep=` line the file had |
| Capabilities | `file_type_table.cpp`: `ods`, `xlsx` and `csv` declare `edit` and `save` |
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
`rich` for a link or anything else past text and line breaks, `shapes` where
the cell is nothing but its anchored drawings), and `data-odr-editable` on
`<body>`. Every other cell is editable, an empty one included.

The editor is an overlay the script places over the cell, so the sheet's DOM
stays untouched until the commit patches the cell. The overlay is a
`textarea`: Alt or Ctrl with Enter breaks a line, as in Excel and Calc, and
the value states it as `\n`. `.ods` writes a `text:p` per line, and `.xlsx`
keeps the `\n` and turns `wrapText` on, as LibreOffice's export does.

**Why:** a `td` holds wrappers and shapes, so a static `contenteditable` is
the wrong tool. Refusal is an event, not a silent no-op: a click on a locked
cell outlines it and calls `odr.onEditRefused` (decision 7).

A sheet that `spreadsheet_limit` or `spreadsheet_cell_limit` cut states its
whole extent in `data-odr-cut`. Enabling the mode refuses with `sheetCut` and
that extent, and so does a move past the rendered extent, with the position it
aimed at.

The mode itself is generic and lives in `editing.js` (`editing.md` decisions
9 to 12). What stays here is the sheet's own: the overlay, the locks, the
position map and the `setCell` op.

### 4. The type follows the content

A strict grammar parses the typed string: optional sign, digits, one `.`,
optional exponent is a number. Anything else is a string. A leading `'` forces
a string, and the editor opens a string cell with one where the grammar would
read it otherwise. `=` is refused with `formulaInput` until the evaluator
exists.

**Why:** the file states the type per cell (`office:value-type`, `c/@t`) and
changes it freely, so a cell has no fixed type. A number cell writes
`office:value` and the `text:p`, or `<v>` alone. A string cell drops
`office:value`, or becomes `t="inlineStr"` with `<is><t>`.

The decimal separator is the one of `Document::locale`, which an `.ods`
states on its default style (`fo:language`, `fo:script`, `fo:country`, or
`style:rfc-language-tag` where those cannot spell it). An
editable sheet page carries it as `data-odr-locale`, and the editor asks
`Intl.NumberFormat` for the separator, so no table of locales ships. The
op states the number with `.` and the text as typed. A point in a German
sheet makes a string, as a comma in an English one does. An `.xlsx` states
no locale and shows its numbers with `.` until number formats are read, so
it keeps `.`.

### 5. Formulas are recomputed in C++, once, and reached through the host

The evaluator runs in C++ only. The browser hands the host the op log per
commit, and gets back the cells whose display changed.

**Why not JavaScript generated from the tree:** two function libraries that
drift. **Why not the engine as wasm inside the HTML:** every current host has
the engine in process. `HtmlConfig::embed_shipped_resources` is where such a
blob would go if a host without a bridge appears.

Where the evaluator has no answer, the file still has to stay honest.
`.xlsx` has a switch for this, `calcPr/@fullCalcOnLoad="1"` (ECMA-376
18.2.2), set on every save. `.ods` has none, so an odf write removes the
cached result of every dependent cell: the value attributes and the
`text:p`. `Document::recalculate` writes back the results it computes. A
cell stating a formula and no result is one a reader has to compute, and
such a cell renders empty here.

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

## Landed

Each item landed as a stack of PRs. The decision numbers are the ones the PRs
and the commits name. Git holds the full text of each decision.

- **Cell formatting** (decisions 9 to 13).
  - `Sheet::set_cell_style`, `set_row_style` and `set_column_style`, and the
    ops `setCellStyle`, `setRowStyle` and `setColumnStyle`. A style is a delta
    with the keys of a run, plus `fill` and `align`.
  - A write copies the style the cell shows and never edits a shared one. An
    `.ods` adds an automatic style, and an `.xlsx` appends a `font`, a `fill`
    and an `xf` only where no equal one exists.
  - A row or column style also reaches the cells that the file does not state
    yet. An `.ods` row style goes onto the cells, because LibreOffice applies
    the `table:default-cell-style-name` of a row to every row.
  - The selection is a rectangle, and the pin is one of its corners. A header
    click selects a row or a column, and an undo step is one gesture.
- **Number formats** (decisions 14 to 16).
  - `internal/number_format` parses a format code as MS-XLS 2.4.126 states
    it, and one formatter writes a number with it. An ods data style is first
    turned into a format code.
  - An `.xlsx` reader formats `<v>` with the `numFmtId` of the cell. An
    `.ods` reader keeps the `text:p`, and the ods writer formats a written
    number with the data style of the cell.
  - An editable render states `data-odr-value`, so the editor opens a
    formatted number on its value.
- **Typed dates and times** (decisions 17 to 19).
  - A date states days since 1899-12-30, and a time states its length in
    days. An `.xlsx` in the 1904 system adds 1462 days.
  - A written date takes the date format of the cell, else a built-in one.
  - The editor reads a date in the order of the document's locale, and ISO
    8601 always.
- **Inserted and deleted rows and columns** (decisions 20 to 27).
  - The ops `insertRows`, `deleteRows`, `insertColumns` and `deleteColumns`
    state a sheet, an index and a count. `formula::SheetEdit` states either
    axis.
  - A reference moves with the cell it names, an absolute one too. A
    reference all inside removed cells becomes `#REF!`.
  - The writers of `.ods`, `.xlsx` and `.csv` move the cells and everything
    the file addresses. An edit refuses before it writes anything where it
    cuts a merge or an xlsx array formula, or pushes a cell off the grid.
  - A new row or column is plain. The grid ends at column 16384 (`XFD`).
  - A structural op splits the log, so no op merges across it. The stale
    marks follow every op of the log in order.
- **What else a structural edit moves** (decision 28).
  - XLSX tables: the range, the filter and the calculated columns move. A new
    column gets a `tableColumn` and a header cell of the same name. An edit
    that removes the header row, the totals row or the whole table refuses.
  - The formulas of conditional formats and validations, and the conditions
    of an `.ods`. A rule whose first cell a delete removes reads from the
    first cell that stays.
  - XLSX page breaks, the ranges of xlsx and ods charts, and the source and
    the place of an xlsx pivot table. An edit that cuts a pivot table, or
    removes all of its source, refuses. A source in another workbook stays.
  - The Excel 2010 extensions of an xlsx worksheet: an `x14` conditional
    format, validation and sparkline. A rule on another sheet moves the
    references it reads in the edited one.
- **Formula evaluation** (decisions 29 to 33).
  - `formula::evaluate` gives a value or no answer, never a wrong value. An
    `.ods` follows LibreOffice and reads `table:calculation-settings`. An
    `.xlsx` follows Excel's documented rules. Where the applications or the
    documentation leave a result open, the evaluator gives no answer.
  - A corpus test evaluates every formula of `test/data/input`, and fails on
    an answer that differs from the result the file caches.
  - `formula::Value` is empty, a number, a string, a boolean, an error, a
    reference or an array. A reference stays one until a function reads it.
    An error propagates left to right. Two numbers are equal where they agree
    to about 15 significant digits, and an `.ods` sum adds as `KahanSum`.
    Where Excel's undocumented tolerance and a reading to 15 digits decide
    apart, an `.xlsx` comparison has no answer.
  - A range reads only the cells its sheet states, through
    `SheetAdapter::sheet_visit_cells`, so a repeated `.ods` run reads its
    value once. A formula nests 64 levels at most, as Excel writes them.
  - An `.ods` spells a boolean in `&` as the LibreOffice in its
    `meta:generator` does: `1` before 27.2, `TRUE` from it, and no answer
    for another application.
  - 116 functions of mathematics, logic, information, text, lookup, the
    conditional aggregates and dates, and the names a document defines. An
    array formula, a volatile function, a reference into another document
    and a localized function name have no answer.
  - `Document::recalculate` computes each stale cell on demand: a dependent
    of an edit, every formula after a structural edit, and a formula without
    a result, with a name or with a volatile function. A cycle gets no
    result. A save recalculates after an edit.
  - A cell without an answer has no result in an `.ods`, and keeps its old
    one in an `.xlsx`, which keeps `fullCalcOnLoad`. Every binding has
    `recalculate`.

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

- **A structural edit leaves a formula that does not parse as it is.** That
  includes a formula nested past 64 levels. The edit should refuse instead,
  so that no reference ends up wrong.
- **Live results in the page.** The page has no evaluator, so it marks the
  formula cells stale until something computes them. There are four ways to
  compute them while the user edits:
  - **A small wasm module with the evaluator only. This is the preferred
    way.** The page has the cells already: `data-odr-formula` and
    `data-odr-value` state what the evaluator reads. So the module holds
    `internal/formula` and `internal/number_format` and nothing else: no
    XML, no zip, no file format. Its only dependency outside core is `fmt`.
    A JavaScript callback implements `formula::CellSource` over the DOM
    (decision 30). The page embeds the module as base64, as
    `HtmlConfig::embed_shipped_resources` does with the other resources,
    so a page with no host computes too: an export, a page opened from
    `file://`, a page sent as a mail attachment. There is still one
    evaluator and one corpus test. The size of the module is not measured
    yet.
  - The host computes them in process (decision 5) and sends them to the
    page. The apps and the npm host have core already. The host must keep a
    decoded document that matches the page, and an undo in the page needs a
    fresh decode and a replay of the log, because `Document::edit` cannot
    take an op back. A page with no host does not compute.
  - The page loads all of core as wasm. The page then needs the file, and
    the module is 4.6 MB (`odr-core.wasm` today).
  - The page has an evaluator in JavaScript. A page with no host computes,
    but this is a second library of functions that drifts from the first,
    which decision 5 rejects.

  An evaluator in the page reads only what the page shows. A formula that
  reads past a sheet the limits cut, or a sheet that is not in the page,
  stays stale (decision 29). The page still needs a host to save.

  The build cost of the small module: a page resource made with emscripten
  makes every build that ships the resources need emscripten too, also the
  android, ios and python builds. The emsdk in a conan cache takes 2.6 GB.
  So CI, which has emscripten for the npm package already, builds the
  module once and publishes it, and the other builds take that file, as
  they take any shipped resource. A build without the module ships pages
  that mark formulas stale, as now. This has two costs:
  - The module must match the core that renders the page, for example in
    how `data-odr-formula` spells a formula. So CI publishes the module per
    core version, and a build takes the one of its version.
  - A local change to `internal/formula` reaches the page only after CI
    builds the module, or where the developer installs emscripten.
- **Formula input in the editor.** The user types `=SUM(A1:B2)`, with `;`
  between arguments where the decimal sign of the locale is `,`. The writer
  parses that spelling with the locale of the document, and writes the
  formula in the syntax of the file. It comes with live results, because a
  typed formula has no result until something computes it.
- **Out of the evaluator so far:** array formulas and dynamic arrays, a
  reference into another document, iterative calculation, the volatile
  functions, and localized function names. Each cell that needs one stays
  without an answer (decision 29).
