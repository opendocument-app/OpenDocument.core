# Spreadsheet editing design

Status: **proposed; nothing scheduled.** This records why spreadsheet editing
is staged the way it is, what the code already gives us, and the order the
steps go in. It is a plan, not a record — update it as steps land.

Related: [`editing.md`](editing.md) is the accepted direction for text
documents (op log, ids, browser-side undo). This builds on its decisions and
takes the pieces a sheet makes cheap first. [`odf/AGENTS.md`](../../src/odr/internal/odf/AGENTS.md)
and [`ooxml/spreadsheet/AGENTS.md`](../../src/odr/internal/ooxml/spreadsheet/AGENTS.md)
describe the read side.

## Problem

Spreadsheets render but do not edit. `odf::Document::is_editable` hardcodes
`false` for `.ods`, `.xlsx` is read-only end to end, and the browser side has
nothing a sheet needs: no way to type into an empty cell, no notion of a
number behind the string, no answer when a cell cannot be edited.

The two complications the formats add over text are well understood:

- **ODS collapses repeats.** One `<table:table-cell
  table:number-columns-repeated="1000"/>` stands for a thousand cells, a row
  can repeat the same way, and an empty cell has no element at all
  (`odf_parser.cpp::is_cell_empty`). A cell to write into may not exist as a
  node yet.
- **XLSX shares strings.** A `t="s"` cell's text is parsed out of
  `sharedStrings.xml`, so the registry's text nodes for that cell live in a
  part every other cell with the same string points at. Writing into them
  edits every one of those cells.

And a spreadsheet adds the thing text never had: **formulas**, whose cached
results go stale the moment an input changes.

## What the code gives us

| Piece | Where | State |
|---|---|---|
| ODS string-cell edit | `odf_document.cpp::text_set_content` | Works: `Document.edit_ods_diff` edits five cells in memory. Only the run's text changes; `office:value` on a number cell is not touched |
| ODS save | `odf_document.cpp::save` | Re-serialises `content.xml`, byte-copies the rest — the same shape a sheet needs |
| ODS cell index | `odf_element_registry.cpp::Sheet::register_cell` | Per row a run of `(end, element_id, node)` entries; repeats collapse onto one entry. Written once at parse; nothing inserts |
| ODS repeated cells | `odf_document.cpp::split_repeat` | A write cuts the run and `reindex_sheet` rebuilds the index (step 2.1, landed) |
| XLSX edit | `sheet_set_cell` | Writes a cell value (step 0.2, landed); `text_set_content` is still a no-op |
| XLSX save | `ooxml_spreadsheet_document.cpp::save` | Writes back the worksheets and `workbook.xml`, copies the rest (step 0.2, landed) |
| XLSX cells | `Sheet.cells` `(col,row) → {node, id}` map | Off-tree; an empty position has no `<c>` node |
| Cell value | `SheetCellAdapter` | `sheet_cell_value` reads the number and the formula (step 0.1, landed); `sheet_cell_value_type` stays the cheap question the renderer asks. Dates, booleans and errors still report `string` |
| Number formats | — | Not parsed in either engine. ODS shows the producer's cached `text:p`; XLSX shows the raw `<v>` (a date is its serial) |
| Formulas | `sheet_cell_value` | The expression is read and handed out as a string (step 0.1, landed); nothing parses or evaluates it. XLSX shows the cached `<v>`, ODS the cached `text:p`. `xls` and `numbers` drop the expression at parse time |
| Browser: sheet script | `frontend.cpp::spreadsheet_js` | Hover/pin, raise a clipped cell over its neighbours, sort rows in the DOM. Sorting reorders `<tr>`s, so a row's identity is its `<th>` label, not its index. Publishes `odr.sheet` (step 1.1, landed), and the value and reflow half of it (steps 1.2/1.3, landed) |
| Browser: editing script | `frontend.cpp::document_js` | A `MutationObserver` over `contenteditable` runs keyed by `data-odr-path`; `odr.generateDiff()` emits the envelope |
| Browser: sheet editor | `frontend.cpp::sheet_editing_js` | `odr.editing` with the mode, the locks and the refusals (step 1.1, landed), and the overlay that types into a cell (steps 1.2/1.3, landed). Undo/redo and `committed()` are step 1.4 |
| Wire format | `document.cpp::Document::edit` | The op envelope, `setCell` and `setText` (step 0.4, landed) |
| Addressing | `DocumentPath` | Already spells a cell by position: `/child:0/cell:A1/...` |
| Capabilities | `file_type_table.cpp` | `ods` and `xlsx` declare `edit` and `save` (step 0.2, landed); `csv` declares neither. `odr_test` checks the declaration against `Document::is_editable` |

## Decisions

### 1. A cell is the unit of editing, addressed by position

The op is `setCell {sheet, column, row, value}`. Not the run's element id, not
a path.

**Why:** the cell a user types into may have no element — every empty cell in
both formats, every repeated cell in ODS — so an id cannot name it. A position
can, and it is what the file itself uses (`r="B3"`, the repeat cursor). It also
side-steps [`editing.md`](editing.md)'s id-stability question entirely for
sheets: nothing is renumbered when the only op replaces a cell's whole content.
Ids stay the answer for text documents, where an insertion point inside a
paragraph has no position of its own.

Coalescing is a map keyed by position, last write wins. Undo keeps the previous
value beside the op. The whole log is idempotent, which decision 5 leans on.

### 2. One op-log envelope replaces `modifiedText`

```json
{
  "version": 1,
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

**Landed.** `Document::edit` is a dispatcher over `ops` and throws on the first
op it cannot apply, leaving the ones before it applied — a document is decoded
fresh by `DocumentFile::document()`, so the host replays onto a copy by
construction; the wasm session, which holds one document, has to replay onto a
fresh decode too. `setText {path, text}` carries what `modifiedText` carried
and is what `generateDiff()` now emits; it gains the id form when
[`editing.md`](editing.md) phase 1 lands. The bindings pass a string through
and did not change.

`version` is the wire version. A document stamp (decision 7 in `editing.md`)
is deferred: a sheet op names a position, and a position is meaningful against
any decode of the same file.

### 3. Editing is a browser mode, not markup

`odr.editing.enable()` / `disable()` turns the mode on; `HtmlConfig::editable`
stops changing what a sheet writes. The page carries only what the browser
cannot work out for itself:

- a **lock** on a cell that cannot be edited, as a class plus its reason —
  `formula`, `repeated` (ODS, until step 2), `rich` (several runs, several
  paragraphs, a link, a line break), `shapes` only where the cell is nothing
  but its anchored drawings;
- whether the **document** can be edited at all, one attribute on the table,
  so `enable()` can refuse with a reason before the user clicks anything.

Everything else — including every empty cell — is editable. The cost is a
class on the locked cells only, nothing on the half million others.

**Why:** the user should not have to translate twice to switch modes, and a
static `contenteditable` is the wrong tool for a cell anyway: a `td` holds
`x-p` wrappers, the raise wrapper, shapes. The editor is an **overlay** the
script places over the cell (as every spreadsheet does), reusing the raise
geometry; the sheet's DOM is untouched until the commit patches the cell.

**Refusal is a first-class event.** Clicking a locked cell, or any cell of a
read-only document, outlines it briefly and calls `odr.onEditRefused` so the
host can say why — a snackbar on mobile. A silent no-op is the frustrating
outcome the mode exists to avoid. Decision 7 is the channel.

### 4. The type follows the content

The typed string is parsed by a strict grammar: optional sign, digits, one `.`,
optional exponent — a number. Anything else is a string. A leading `'` forces
a string, as every spreadsheet does. `=` is refused in step 1 (formula input
comes with step 4).

**Why not keep a number cell numeric:** telling the user "this cell holds a
number" is a rule no spreadsheet has, and the file has no such rule either —
`office:value-type` and `c/@t` are per cell and change freely. Letting the
type follow keeps both the value and its string right by construction: a number
cell writes `office:value` *and* the `text:p`, or `<v>` alone; a string cell
drops `office:value`, or becomes `t="inlineStr"` with `<is><t>`.

The one thing the file has that we lack is the **number format**: a cell
formatted `€ 1.234,50` and edited to `2000` shows `2000` until the file is
reopened, where the producer formats it. Step 1 accepts that and the overlay
says it (the raw string is what the user typed). Parsing number formats is
step 5, and is a read-side gain on its own — `.xlsx` shows raw serials today.

Decimal comma: step 1 parses `.` only. The document's locale is not read
anywhere; see open questions.

### 5. Formulas are recomputed in C++, once, and reached through the host

Step 1 locks formula cells and lets their cached results go stale. Step 3
parses formulas for their *references* only, which is enough to mark the
dependents stale in the view and to keep the file honest (below). Step 4 adds
the evaluator.

When it exists, the evaluator runs in C++ and nowhere else. The browser asks
the host — the WebView bridge on droid/ios, the worker on wasm — with the op
log, and gets back the cells whose display changed. Not per keystroke: per
commit, and only when the edited cell has dependents.

**Why not JavaScript generated from the expression tree:** two evaluators,
one per language, with the function library — `SUM`, `VLOOKUP`, date
arithmetic, error propagation — written twice and drifting. The drift
`editing.md` accepts for op *replay* is a few tree edits; a formula engine is
hundreds of functions.

**Why not ship the engine as wasm inside the HTML:** it is the right answer
for a host with no bridge at all, and it is the *same* C++, so the door stays
open. But it costs an emscripten build inside every platform build (the bytes
have to be compiled into the library to be written beside the document), and
no current host needs it — droid, ios and the npm package all have the engine
in process. Revisit when a static host appears. The `embed_shipped_resources`
mechanism is where such a blob would go.

**The file has to stay honest without our engine.** An edited input leaves
cached formula results wrong in the saved file. `.xlsx` has a switch for
exactly this: `workbook.xml` `calcPr/@fullCalcOnLoad="1"` (ECMA-376
18.2.2), set on any edited workbook. `.ods` has no such switch, and
LibreOffice trusts a file its own generator wrote — whether it recomputes a
formula cell whose cached value we *remove* is the first spike below.

### 6. The page stays up until the user saves

The host holds the log the page hands out (`odr.editing.getOperations()`),
the page is never re-translated for a save, and after a successful save the
host tells the page (`odr.editing.committed()`) so the log resets and undo
starts over with the file the page now matches. The `sheet{index}.html` views
each run their own script, so a host showing sheets as separate pages collects
a log per view; every op carries its sheet, so concatenation is the merge.

### 7. Host events are flat `odr.on*` callbacks, and the host owns the wording

The page calls out; the host listens. Commands live on `odr.editing` the way
`odr.annotation` holds the annotator's, and events stay flat on `odr`,
following `odr.onError` and `odr.onZoomChange`:

```js
// {sheet, column, row, reason, code, message}
odr.onEditRefused = function (event) {};
// {dirty, operations, canUndo, canRedo}
odr.onEditChange = function (event) {};
// {editing, editable, reason, code, message}
odr.onEditModeChange = function (event) {};
```

**The message is for the console; the code is for the host.** A mobile
snackbar is written in the app's own string catalogue, and nothing in this
library is localised — so the host maps `code` to its wording, and `reason`
(`"formula"`, `"formulaInput"`, `"repeated"`, `"rich"`, `"readOnly"`,
`"encrypted"`, `"cut"`)
is the same thing spelled for a reader of the log. We still ship an English
`message`, so a developer who wires nothing sees it in the console (the
`odr.onError` default does exactly this) and a desktop host with no catalogue
can show it as it stands.

**Codes are appended, never renumbered**, and share one space with
`odr.onError`'s — `errorIllegalEditNewLine` holds 1. The rule the wasm enum
ordinals already live under: appending stays silent, reordering goes loud.
Pin them in `test/browser/sheet` the way `tests/enums.test.mjs` pins the
enums.

**One object argument, never positional.** `onError(code, message)` cannot
grow a field without breaking every host that implements it; an object can.
Every callback added from here takes one.

**The page suppresses its own repeats.** Tapping a locked cell four times is
one snackbar, not four: an identical refusal within a couple of seconds of the
last is dropped by the page, which knows what it just fired. Cheaper here than
in three hosts.

**`onEditChange` is what decision 6 needs.** `dirty` is how the app lights its
save button and warns on back-press while the page holds unsaved edits;
`canUndo`/`canRedo` drive the toolbar. It fires on every commit, undo, redo
and on `committed()`.

**Attaching**, per host:

- **droid / ios**: the WebView loads the view at the top level, so the host
  assigns the callbacks once the page has finished loading
  (`evaluateJavascript` / `evaluateJavaScript`) and hops to the UI thread to
  show the snackbar. The scripts are written at the end of `<body>`, so the
  load event is late enough.
- **Browser / npm**: the view is an iframe, and the wasm example already
  renders it `allow-same-origin`, so the embedder assigns on
  `contentWindow.odr` at the frame's `load`. A cross-origin sandboxed frame
  cannot be reached this way and is out of scope — `postMessage` if one ever
  appears.
- No C++ is involved: these are page-to-host, so the wasm rule about callbacks
  being worker-local and synchronous does not apply to them.

**Why not reuse `odr.onError`:** a refusal is expected UX, not a fault. It is
frequent, it carries a position, and a host wants it on a snackbar while a real
error goes to a dialog or a log. Sharing the code table keeps one lookup for
both.

### 8. The sheet script owns the position map, and publishes it as `odr.sheet`

The editor is a second script on the page, and the two things it needs first —
which cell a position names, and what is pinned — belong to the first, which
already owns the pin, the raise and the sort:

```js
odr.sheet.cellAt(column, row); // the `td`, null past the sheet's extent
odr.sheet.positionOf(cell);    // {column, row}, null for a header
odr.sheet.pinned();            // {column, row, cell}, null for none
odr.sheet.pin(position);       // null clears; false where there is no cell
odr.sheet.lower();             // puts back a cell the pin raised
odr.sheet.valueAt(column, row);       // what the page shows, as an op states it
odr.sheet.showValue(column, row, v);  // shows it, and reflows the row
odr.sheet.reflow(row);                // the spill geometry, measured again
```

**Why not a copy in the editor:** the map is not a walk over `colspan`. A row
is named by its `<th>` label, because sorting moves the `<tr>`s away from
position order; a `rowspan` from an earlier row leaves the positions it covers
unwritten, so a colspan-only walk misreads every cell after them; and it is
built once, which means the script that reorders rows is the one that has to
know. Two copies would also be two owners of the pin classes and the raise
wrapper — an editor whose overlay is open while the other script lowers the
cell underneath it.

**Why not one script instead:** the read-only view would carry the editor it
never runs, and a raw string literal caps at 16380 bytes on msvc
(`fits_a_literal`), which the two together would reach during step 1.

**The coordinates are the ones an op names** (decision 1), never a DOM index.
The wash paints through `nth-child`, so the ruler's index stays private to the
script, and a merged sheet still gets no wash and no sort control. A position a
merge covers answers with the cell covering it — the one the file states and an
op names.

**The cost is a public surface**, which a host keeps once it ships. It is a
small one, and a host gets scroll-to-cell and "what is selected" out of it. What
step 1.3 needs to reflow a row after a commit (`visibleRight`, `cutOff`) sits in
the same closure and joins `odr.sheet` when it is written, rather than being
reached around.

## Staging

Each step ships on its own. "Both" means `.ods` and `.xlsx`.

### Step 0 — Foundation, C++ only

1. **Landed.** `SheetCell::value()` → `CellValue`: the type, the number where
   the file states one, the text showing it, and the formula where it states
   one. Abstract hook `sheet_cell_value`, filled by odf, ooxml and csv; `xls`
   and `numbers` state the type alone and let `SheetCell::value` collect the
   text off the children, which is what every engine gets for free. This is
   also what a later sort script needs instead of parsing the rendered text.

   `CellValue` is **one type for reading and writing** — immutable, built by
   explicit constructors from a text, a number, or a bare type, composed
   further with the `with_*` withers a decoder needs, and read through getters
   that throw `ValueNotStated` rather than hand back an empty optional. What a
   cell reads as is what writing it back takes.
2. **Landed.** `sheet_set_cell(sheet_id, column, row, CellValue)`, position-
   addressed, behind `Sheet::set_cell` and `::clear_cell`. ODS writes
   `office:value-type`, `office:value` and the `text:p`, through the cell's one
   text run. XLSX rewrites the `c` — `<v>` for a number, `t="inlineStr"` with
   `<is><t>` for a string — and hands the registry a fresh text element; the
   old ones keep their ids and stop being reachable. A shared string is never
   written back into `sharedStrings.xml`, which is what `inlineStr` is for.
   Refused, rather than written badly: a cell the file spells no element for, a
   covered one (XLSX), one holding a formula, and one holding richer markup
   than a single plain paragraph. Every refusal is decided before the engine
   writes anything. **Writing a formula cell waits for step 4** — overwriting
   one leaves every value computed from it stale. A repeated ODS cell was
   refused here and is written since step 2.1.
3. **Landed.** XLSX `save`, mirroring docx: write back every worksheet and
   `workbook.xml` from their dom, byte-copy the rest, and put back the xml
   declaration pugixml never parsed. `fullCalcOnLoad` is set on every save
   rather than only after an edit — we rewrote the file and compute no formula,
   so the reader is asked to.
4. **Landed.** The op envelope and dispatcher in `Document::edit`, with
   `setCell` and a path-addressed `setText`; `modifiedText` dropped
   (**Breaking**, wire only). Coalescing writes here is also what would let a
   batch of ODS writes reindex once rather than once per write — the reindex
   costs about 0.18 us per row node per write, so 100 writes on a 20000-row
   sheet is 0.36 s today.
5. **Landed.** `Document::is_editable` true for both; capability rows gained
   `edit` (`xlsx` also `save`); `odr_test` keeps them honest.
6. **Landed.** `translate_sheet` writes its cells through a `WritingState`
   whose `editable_markup` is false, so no run carries `contenteditable` and
   `plain_text` folds it into the `td` as it does read-only. It could not be
   gated on `Document::is_editable`, which item 5 makes *true* for a sheet.
7. **Landed.** Tests: set a number, a string, clear a cell, and each refusal,
   on both formats, from inline fixtures; save and reopen. The LibreOffice
   oracle (`soffice --convert-to`) stays a by-hand check — it is not in CI, and
   it is the only one that says a written package is really valid.

### Step 1 — The browser editor

1. **Landed.** `odr.editing` mode: enable/disable, lock classes and the
   document attribute from `translate_sheet`, and the three `odr.on*` callbacks
   with their code table (decision 7). `spreadsheet_js` publishes `odr.sheet` in
   the same step (decision 8) — the position map the mode reads a lock through.
2. **Landed**, with item 3: an editor that drops what is typed is not one.
   Overlay editor: double-click / Enter / typing opens it over the cell; Enter,
   Tab and blur commit; Escape cancels; arrow keys move the pin, through
   `odr.sheet.pin` rather than a pin of its own. A locked cell refuses on the
   click rather than on the double click that would have opened it.
3. **Landed.** Commit: parse per decision 4, record the op with its inverse,
   patch the cell — text, `odr-value-type-float` for alignment, keep any shapes
   in A1 — and **reflow the row**: the spill and clip `translate_sheet` measured
   for the neighbours (`clip-path:inset`, `overflow:hidden`) are stale once a
   blank cell fills or a full one empties. `odr.sheet` gained `valueAt`,
   `showValue` and `reflow` for it, and `getOperations()` came with them: a log
   nothing hands out is a log nothing can check.
4. Undo/redo over the in-memory log; `committed()`; both raise `onEditChange`,
   which is what a host's save button and back-press warning read.
5. `test/browser/sheet` grows the editing cases; the wasm example gets an
   edit-and-save button, which is also the host-wiring reference for droid/ios.

### Step 2 — Materialise the cells that are not there

1. **Landed for repeated cells.** ODS repeat splitting: a write into a run of
   `n` repeated cells becomes left (`k`), the cell, right (`n-k-1`), a repeated
   row cloned the same way first, the original node staying as the one written
   so its element survives. The index is not patched in place — `reindex_sheet`
   rebuilds it off the dom, a cell node keeping the element it carries — which
   avoids a second copy of the parser's row loop. The `repeated` lock is gone.

   **Open:** the same primitive for a position the file states no element for.
   A run with a node but no element (`<table:table-cell number-columns-repeated=
   "1000"/>`) only needs the split plus a `text:p`; a position past the row's
   last cell or the sheet's last row needs appending and growing the extent.
2. XLSX: insert `<c r="…">` in column order into its `<row>`, create the
   `<row>` in row order, grow `<dimension ref>`.
3. Rich cells: replace with one plain paragraph, keeping the cell style. The
   `rich` lock stays on a cell with a link or a line break; it goes for
   several runs of the same paragraph.

### Step 3 — Formulas, read side

1. Parse both syntaxes into one AST: OpenFormula (`of:=SUM([.A1:.B2])`,
   `table:formula`) and OOXML (`SUM(A1:B2)`, `<f>`, shared and array
   formulas). References, ranges, sheet-qualified references, named ranges
   left as opaque.
2. Reference extraction → dependency graph per document; `Document` answers
   "which cells depend on this position".
3. View: a commit marks dependents stale (a class, the host is told); the
   locked formula cell exposes its text (`data-odr-formula`, formula cells
   only) so a formula bar or a tooltip can show it.
4. File: the `.ods` answer from the spike — drop the cached value of dirty
   dependents, or whatever LibreOffice needs to recompute.

### Step 4 — Formulas, evaluate

1. Evaluator over the AST with a typed value (number, string, boolean, error,
   empty), error propagation, and a function library opened with the
   frequent thirty or so (arithmetic, `SUM`/`AVERAGE`/`MIN`/`MAX`/`COUNT`,
   `IF`/`AND`/`OR`, `ROUND`, `CONCATENATE`, `VLOOKUP`/`INDEX`/`MATCH`,
   `TODAY`/`DATE` with the 1900/1904 epochs). An unknown function leaves the
   cached value and flags the cell rather than guessing.
2. Incremental: recompute the dirty set in topological order, cycles detected
   and reported as `#REF!`-style errors.
3. `Document::recalculate(operations) → changed cells` (position, display
   string, kind) — the query the host relays per decision 5; writing cached
   results on save.
4. Formula input in the editor (`=`), with the parse error surfaced.

### Step 5 — Later

- Number formats (`number:number-style`, `numFmt`) for display, dates and
  booleans as their own kinds; fixes `.xlsx` serials on the read side too.
- Multi-line cells (Alt+Enter), cell style edits, insert/delete rows and
  columns (the moment ids for rows appear, `editing.md`'s append-only rules
  apply).
- `.csv`: `save` is a serialiser and the cell op fits its packed ids; cheap
  once the envelope exists, low value.
- The wasm-in-HTML engine, if a bridge-less host appears.

## Low-hanging fruit

Ordered by value over cost; all in step 0 or 1.

- **XLSX save** — the docx save with three path names changed.
- **ODS number sync** — `office:value` beside the text, a few lines in
  `text_set_content`'s successor.
- **`fullCalcOnLoad`** — one attribute, and the file stops lying after an
  edit.
- **Lock classes + refusal event** — the feedback the mode needs, cheap to
  emit, and the read-side view gains a marker for formula cells. The callback
  is three assignments on each host, and the same channel then carries the
  dirty flag the save button needs.
- **`SheetCell::value()`** — a missing read accessor; the sort script and any
  binding user wants it regardless of editing.
- **The `contenteditable` gate** — one condition, removes 594 attributes from
  a reference `.ods` and a layout difference between the two modes.

## Complications to budget for

- **ODS repeat splitting is the write primitive**, and the run index was built
  to be written once. Design the insert before step 2, and keep ids
  append-only (`editing.md` decision 4).
- **XLSX shared strings**: converting to `inlineStr` is self-contained but the
  cell's registry subtree points into `sharedStrings.xml` — it must be
  rebuilt, not patched. Verify with the oracle that a workbook mixing
  `inlineStr` and shared cells round-trips.
- **Stale formula results in the file** for `.ods` (the spike). Until step 4
  there is no way to write a correct value.
- **Row reflow after a commit**: the spill/clip geometry is computed at
  translate time from the neighbours; the browser has to redo it for the
  edited row. Without it an edit into a blank cell shows the left neighbour's
  overflow painting across the new text.
- **Sheets past the cut** (`spreadsheet_limit`, `spreadsheet_cell_limit`) are
  not in the page and cannot be edited; the mode should say so where a view
  reports a `sheet_cut`.
- **Several views, one log**: the host merges; a save with a partial log is
  a partial save. The wasm package can hide this in the session.
- **Encrypted packages** are not savable (`is_savable` false after decrypt);
  `enable()` refuses on the document attribute rather than after typing.
- **Decimal separator and locale** are read nowhere; a german user typing
  `1,5` gets a string in step 1.
- **A1 anchors every shape** (`anchors_shapes`): the commit patch must keep
  the shape nodes and replace only the text.
- **Sort and edit together**: sorting reorders `<tr>`s in the DOM and keeps an
  `original` snapshot; an edit patches the `<tr>` in place, so both survive,
  but the position must come from the row label.

## Spikes before step 0

1. **LibreOffice and a formula cell without a cached value** in an `.ods` it
   generated itself: does it recompute on load, or show empty? Also what it
   does when `meta:generator` is ours. Build the probe with `soffice
   --convert-to`, then hand-edit `content.xml`; render and round-trip before
   trusting the spec.
2. **Excel/LibreOffice on a mixed `inlineStr` workbook** — expected fine,
   worth ten minutes.
3. **The ODS run-index insert**: sketch `Sheet::insert_cell` against
   `register_cell` and the `SortedSideTable` (ids appended out of position
   order are fine for a hashed table, not a sorted one — `m_sheet_cells` is
   sorted by id, and new ids are larger, so it holds).

## Open questions

- Should a string cell that receives a numeric-looking string stay a string?
  Real spreadsheets say no; a phone-number column says yes. The `'` escape is
  the compromise for step 1.
- Where does the document locale come from for the decimal separator —
  `settings.xml`, the number format, the host?
- Does the read-only document attribute belong on the table or in a
  page-level `data-odr-*` block the text editor will want too?
- Should the refusal codes be generated from one C++ table so the bindings can
  hand a host the same list, rather than living only in the emitted script?
