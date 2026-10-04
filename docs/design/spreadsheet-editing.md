# Spreadsheet editing design

Status: cells edit and save in `.ods`, `.xlsx` and `.csv`, formulas parse and
track their dependents, and nothing evaluates a formula yet.

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
| Dependencies | `internal::SheetDependencies`, built off the decoded document and again after a row edit. `Document::dependents` and `Document::unresolved_formulas` expose it |
| Row edits | `formula::RowEdit` and `formula::move_rows` move a reference, `formula::move_row_addresses` an address list. `odf_sheet_references.cpp` and `ooxml_spreadsheet_references.cpp` walk what each file addresses |
| Stale results | An odf write drops the cached result of every dependent (`drop_stale_results`). An ooxml write keeps them, because every save sets `fullCalcOnLoad` |
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

Number formats are not parsed, so a formatted cell edited to `2000` shows
`2000` until a producer reopens the file.

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

## Cell formatting

Status: landed. `.ods` and `.xlsx` write a cell style, the sheet editor
formats a selection, and the bindings take `Sheet::set_cell_style`. The steps
landed as a stack, in this order:

1. The xlsx reader reads what the writer writes: a solid fill from `fgColor`,
   theme colours with their `tint`, italic, underline and strikethrough, and
   left and right alignment.
2. `Sheet::set_cell_style` and the `setCellStyle` op, written into `.ods`.
3. The same op written into `.xlsx`.
4. The sheet editor formats a selection of cells, and a selection can be a
   rectangle.
5. `Sheet::set_cell_style` in the python, java, objective-c and npm bindings.

### 9. One op per cell, the same keys as a run

```json
{"op": "setCellStyle", "sheet": 0, "column": 1, "row": 2,
 "style": {"fill": "#ffff00", "bold": true, "align": "center"}}
```

| Key | Value | `.ods` cell style | `.xlsx` |
|---|---|---|---|
| `fill` | `#rrggbb` or null | `fo:background-color` in `style:table-cell-properties`, null is `transparent` | a `patternFill` `solid` with `fgColor`, null is `none` |
| `bold`, `italic`, `underline`, `strikethrough` | a bool | `style:text-properties`, as for a run | `b`, `i`, `u`, `strike` in a `font` |
| `color` | `#rrggbb` | `fo:color` | `font/color/@rgb` |
| `size` | a length | `fo:font-size` | `font/sz`, in points |
| `align` | `left`, `center` or `right`, null for the value type | `fo:text-align` in `style:paragraph-properties` and `style:text-align-source="fix"`, null is `value-type` and no `fo:text-align` | `alignment/@horizontal`, null is `general` |

The text keys are the ones of `setTextStyle`, and off is written, never
removed (`document-editing.md` decision 9). A cell has no `highlight`: `fill` is the
cell's ground.

**Why one op per cell:** a position is how every other sheet op addresses a
cell, and coalescing stays a map keyed by position where the later keys win.
A rectangle is as many ops as the page shows cells, so a selection never
reaches past the rendered extent. The writer keeps one new style per distinct
result for the length of a replay, so a thousand ops add one style.

### 10. A style edit writes a copy, never the style it read

An `.ods` cell style and an `.xlsx` `xf` are shared by every cell naming them.

- `.ods`: the writer claims the cell (`claim_cell` or `grow_to_cell`), adds a
  `style:family="table-cell"` automatic style that copies the one the cell
  shows (its own, else the row's or the column's default) with the delta
  applied, and points `table:style-name` at it. A formula cell and a rich cell
  take a style: nothing in their content changes. A covered cell refuses.
- `.xlsx`: the writer appends a `font`, a `fill` and an `xf`, each only where
  no equal one exists, with `applyFont`, `applyFill` and `applyAlignment` set,
  sets `c/@s`, and saves `styles.xml`. A cell the file does not state is made
  as for a value (`insert_cell`).

### 11. The API takes the two style types the read side returns

`Sheet::set_cell_style(column, row, TableCellStyle, TextStyle)`. The set
fields are the delta, and a fill of alpha 0 is `fill: null`, as a highlight
is for a run. `HorizontalAlign::general` is `align: null`. Only a write takes
it: a reader reports an alignment by value type as none.

### 12. A selection is a rectangle, and the pin is one corner of it

`spreadsheet.js` owns it as it owns the pin: shift with a click or an arrow
key, and a drag with a mouse in the editing mode, span it from the pin.
`odr.sheet.selection()` answers `{columns: [first, last], rows: [first, last],
focus}`, `odr.sheet.select(position)` moves the focus, and a header click
selects its row or its column across the rendered extent. Ctrl or Cmd with B,
I and U toggle, where the config gives the scripts the shortcuts. The editor
reports the keys the selected cells agree on through `onSelectionChange`, as the
document editor does, and `odr.editing.format` and `toggle` act on every
unlocked cell of it. A lock refuses a value, not a style.

The editor patches the `td` and the text inside it with the declarations the
renderer writes, and a fill also sets `--odr-dark-fill` with the same mapping
as `html::dark_fill`, so a fill made in the dark scheme shows. The renderer
writes `--odr-fill` beside it, so the editor reads a cell's fill back in the
dark scheme too. `html_common` and `formatting.html` pin the same colours.
Undo holds each cell's inline style before the gesture, and one gesture is one
undo step.

### 13. A row or a column style reaches the cells no file states

```json
{"op": "setRowStyle", "sheet": 0, "row": 2, "style": {"fill": "#ffff00"}}
{"op": "setColumnStyle", "sheet": 0, "column": 1, "style": {"bold": true}}
```

`Sheet::set_row_style` and `Sheet::set_column_style` take the same delta as
`set_cell_style`. A cell that states its own style takes the delta on that
style, so it keeps what it showed.

- `.ods`, a column: the delta goes onto the column's
  `table:default-cell-style-name`, which every cell without its own style
  takes. A cell of the column that states a style, or that sits in a row
  stating a default, gets its own copy. A repeated row is not cut, because
  its cell stands for every row of it.
- `.ods`, a row: the delta goes onto every cell of the row, padded to the
  last declared column, and each one starts from the default of its column.
  The row's `table:default-cell-style-name` is not written: LibreOffice
  applies it to every row of the sheet.
- `.xlsx`, a row: the delta goes onto the row's `s` with `customFormat`, and
  onto every `c` of it. A column stating a `style` inside the used range gets
  a `c` where the row states none, because the row's format would hide the
  column's (ECMA-376 18.3.1.4).
- `.xlsx`, a column: the `col` is cut out of the one covering it, or stated
  with `sheetFormatPr/@defaultColWidth`, because a `col` without a width is
  zero wide in Excel. The delta goes onto its `style` and onto every `c` of
  it, and a row with `customFormat` gets a `c` where it states none.

**Why:** a header click selects a row or a column across the rendered extent
only. The cells past it, and the cells a later write makes, have to take the
style too.

The sheet editor writes one `setRowStyle` or `setColumnStyle` for a header
selection, and paints the cells the page has. Coalescing still merges the
keys of one position, but a style op never merges back past a later one that
reaches a cell in common, so a cell style after a row style stays after it.

## Number formats

Status: landed. The steps landed as a stack, in this order:

1. The format-code parser and the formatter for numbers.
2. Dates and times in the formatter.
3. The xlsx reader shows a cell formatted, and types a date as a date.
4. The ods writer formats a number written into a cell with a data style.
5. The sheet editor opens a formatted number on its value.

### 14. One format model, parsed from a format code

`internal/number_format` parses a format code as MS-XLS 2.4.126 states its
grammar: up to four sections, a condition, a colour, `0`, `#`, `?`, the
decimal point, grouping and scaling commas, `%`, `E+`, fractions, `@`,
quoted and escaped literals, `_` and `*`, and the date and time tokens. One
formatter turns a number and a model into the text a cell shows.

An ods data style (`number:number-style`, `number:currency-style`,
`number:percentage-style`, `number:date-style`, `number:time-style`,
`number:boolean-style`, `number:text-style`, with its `style:map`s) is turned
into a format code first, as LibreOffice does when it exports one. So there is
one formatter and one set of tests.

**Why not one model per format:** the two languages say the same things, and
a format code is the smaller of them to write a test in.

The format code spells `.` and `,` whatever the locale; the formatter writes
the locale's signs for them, from a small table of languages. The month and
day names come from a table of the same languages, which
`tools/number_format/generate_calendar_names.py` makes from what LibreOffice
shows. A full month name after a day takes the form the language declines it
to, `15 марта` against `Март`. LibreOffice also declines a month before a day
in some languages, and the table does not hold that form. A colour is parsed
and not shown, and `*` fills nothing.

### 15. A reader formats what the file does not show already

- `.xlsx` states the value alone (`<v>`), so the reader formats it with the
  cell's `numFmtId`: a built-in one from ECMA-376 18.8.30 in its en-US
  spelling, or a `numFmt` of `styles.xml`. `CellValue` keeps the number and
  states the formatted text. A date or time format types the cell
  `ValueType::date` or `time`, its number the serial, in the 1900 system or
  in the 1904 one where `workbookPr/@date1904` says so. A boolean shows
  `TRUE` or `FALSE`.
- `.ods` states the value and its rendering (`text:p`), so the reader keeps
  the rendering. The formatter is for the writer: a number written into a cell
  whose style names a data style gets its `text:p` from the style, not from
  the typed text. A cell without a data style keeps the typed text.

### 16. The editor opens a formatted number on its value

An editable render states `data-odr-value` on a number cell whose shown text
is not the plain spelling of its value. The overlay opens on that value, with
the decimal separator of the locale, and a commit compares numbers, not
text. So `€1.234,50` opens as `1234,5`, and a commit of it unchanged writes
nothing.

After a commit the page shows the typed text until the host renders again:
the formatter runs in C++ only, as the evaluator will (decision 5).

## Typed dates and times

Status: landed. The steps landed as a stack, in this order:

1. A date or time value states its serial, and the op carries one.
2. The ods writer writes a date and a time.
3. The xlsx writer writes them, and gives the cell a date format.
4. The sheet editor reads a typed date or time, and opens a date cell on it.

### 17. A date is a serial counted from 1899-12-30

`CellValue` of `ValueType::date` states its number as days since 1899-12-30,
the time of day in the fraction, whatever the file counts from. This is the
1900 system from 1900-03-01 on. A `time` is a duration and states its length
in days, which no epoch moves.

- `.ods` states an ISO 8601 date in `office:date-value` and a duration in
  `office:time-value`. The date is absolute, so `table:null-date`, which only
  says how a formula counts, does not move it.
- `.xlsx` states a serial. A 1904 workbook adds 1462 days. A 1900 serial
  before 61 adds one, because 1900-02-29 never was, and 60 stays on 1900-03-01.

The op is `{"type": "date", "number": 45658, "text": "1/1/2025"}`, and
`"time"` the same.

**Why one epoch:** a host or the editor then computes a date once, whatever
the file.

### 18. A written date takes the format the cell has, or gets one

- `.ods` writes `office:value-type="date"` with `office:date-value`, or
  `"time"` with `office:time-value`. The `text:p` comes from the cell's date
  or time data style, else from the typed text.
- `.xlsx` writes the serial in the workbook's epoch. A cell whose format is
  not a date or time format gets a built-in one: 14 for a date, 20 or 21 for
  a time, 22 for both, as Excel does when a date is typed.

### 19. The editor reads a date in the locale's order

ISO 8601 (`2025-01-02`) always reads as a date. Otherwise the order of day,
month and year and the separator are the ones `Intl.DateTimeFormat` writes for
the document's locale, else the reader's, since a file that states none has
no order of its own, so `1/2/2025` is 2 January in `en-US` and 1 February in
`en-GB`. A two-digit year below 30 is 20xx, else 19xx, as Excel reads one. A
time is `h:mm`, `h:mm:ss`, with `AM`/`PM`, alone or after a date. A time with a
sign is a negative duration.

An editable render states `data-odr-value` on a date or time cell, so the
editor opens the cell on the locale's spelling of its serial, and a commit of
it unchanged writes nothing.

A date written into an ods cell takes the month and day names of its data
style's language (decision 14). An xlsx date takes the names of the language
its format code states, `[$-419]`, and English ones where it states none, as
the file does not say what the reader's system writes.

## Inserted and deleted rows

Status: landed. The steps landed as a stack, in this order:

1. `internal/formula` moves the references of a formula for an inserted or a
   deleted row.
2. `Sheet::insert_rows`, `Sheet::delete_rows` and their ops, written into
   `.ods`.
3. The same written into `.xlsx`: the cells, the formulas, the names and the
   merges.
4. The rest of what an xlsx worksheet addresses: conditional formats,
   validations, links, the filter, the view, drawings and comments.
5. The same written into `.csv`.
6. The sheet editor inserts and deletes the selected rows.
7. `Sheet::insert_rows` and `Sheet::delete_rows` in the python, java,
   objective-c and npm bindings.

Columns follow the same path after this, with the same decisions turned by
ninety degrees.

### 20. Two ops, each a row and a count

```json
{"op": "insertRows", "sheet": 0, "row": 2, "count": 1}
{"op": "deleteRows", "sheet": 0, "row": 2, "count": 3}
```

`insertRows` moves the rows from `row` on down by `count`, and the new rows
are empty. `deleteRows` removes `count` rows from `row` on, and moves the
rows below up. `Sheet::insert_rows(row, count)` and
`Sheet::delete_rows(row, count)` are the API, and a count of zero does
nothing.

A new row is plain: its cells take the default of their column, and the row
the default height. Excel and LibreOffice give it the format of the row above,
but the page cannot know what that resolves to, and an edit that the page
shows one way and the file states another is worse than a plain row.

**Why a count:** a selection of rows is one op, so one undo step is one op,
and a writer cuts a repeated run once.

### 21. A reference moves with the cell it names

Every formula of the document that names the edited sheet moves, on every
sheet, with the formula's own sheet where it names none.

- An insert moves each corner of a reference at or past `row` down by
  `count`, absolute or not, as a cut and paste does. So a range grows by an
  insert inside it and by one at its first row moves, and an insert right
  after its last row leaves it, as Excel and LibreOffice do.
- A delete moves each corner past the removed rows up by `count`. A corner
  inside them moves to the edge of the rows that stay. A cell reference
  inside them, and a range all inside them, becomes `#REF!`.
- A whole-column reference (`A:A`) does not move, and a whole-row one
  (`3:5`) moves as a range does.
- A reference into another document never moves.

A formula is written back only where a reference in it moved, so the
others keep their spelling. A formula that does not parse stays as it is.

**Why not `formula::shift`:** a shift copies a formula to another cell and
keeps an absolute axis. A structural edit moves the cell itself, and `$A$5`
names that cell as `A5` does.

The dependency graph is built again after a structural op, off the moved
formulas, the next time an edit asks it.

### 22. A writer moves what the file addresses, and refuses a cut merge

- `.ods` states rows in order, so an insert cuts the repeated run it falls
  in and puts empty rows there, and a delete cuts the runs at both ends and
  removes what is between. A sheet that ends in an empty repeated run gives
  up as many rows as an insert adds, so its extent stays the same. The
  named ranges and expressions, the print ranges, and every attribute that
  states a cell or a range address move: the end cell of a drawing, a
  conditional format's range, a validation's base cell and a database range.
- `.xlsx` numbers every `row` and `c`, so the writer states the numbers
  again past the edit. The `dimension`, the merges, the defined names and
  every formula move. A shared formula whose members would read something
  else after the move is written out as one formula per cell. The entries
  of `calcChain.xml` move with their cells, and the ones in removed rows go.
  Step 4 moves the ranges of the conditional formats, the validations, the
  links and the filter, the selection and the pane of the view, the drawing
  anchors and the comments.
- `.csv` inserts empty lines and removes lines.

An edit refuses (`UnsupportedOperation`) before it writes anything:

- where an insert falls strictly inside a merge, or a delete removes part of
  one but not all of it;
- where an `.xlsx` array formula reaches over the edge of the edit;
- where an insert would push a stated cell past the last row of the grid.

The formulas inside a conditional format or a validation condition, the
ranges a chart reads and a pivot table's source do not move yet.

### 23. The sheet editor acts on the selected rows

`odr.editing.insertRows(where)` inserts as many rows as the selection spans,
`"above"` it by default or `"below"` it. `odr.editing.deleteRows()` removes
the rows of the selection. Where the config gives the scripts the shortcuts,
Ctrl or Cmd with Shift and `+` inserts above and Ctrl or Cmd with `-`
deletes, on a header selection only, as in Excel. A merge that the edit cuts
refuses with `unsupportedEdit`.

`spreadsheet.js` owns the rows of the page, so `odr.sheet` gains
`insertRows(row, count)` and `deleteRows(row, count)`. They put empty rows in
the page or take the rows out, state the labels of the rows below again, and
rebuild the position map. A deleted row is kept aside for an undo. After a
sort, a new row goes where the row it names is shown.

### 24. A structural op splits the log

Coalescing keys a cell by position, and an insert moves the positions. So a
structural op is a barrier: no op merges with one across it, and the log
stays in order. An undo of an insert removes the rows, and an undo of a
delete puts the kept rows back.

The stale marks walk the coalesced log in order. A write marks the formula
cells that read it, a delete marks the ones that read a removed row, an
insert the ones whose range it grows, and each structural op moves what every
formula cell reads and where it sits, as decision 21 states. The marks then
spread to the cells that read a marked cell. A formula cell keeps the
spelling of its formula in the page until the host renders again.

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
- Inserted and deleted columns, on the path the rows took (decisions 20 to
  24).
- The formulas inside a conditional format or a validation condition, the
  ranges a chart reads, a pivot table's source and an xlsx table, which a row
  edit leaves where they were.
