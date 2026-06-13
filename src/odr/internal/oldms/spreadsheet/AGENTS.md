# `.xls` (Excel / BIFF8) support — status, design & open work

What the `oldms/spreadsheet/` module does **today**, the **design decisions**
behind it, and the **open work**. Shared `oldms/` conventions are in
[`../AGENTS.md`](../AGENTS.md).

**Scope.** Extract the **visible cell text** of every worksheet and expose it
through the abstract document model so the generic HTML renderer produces a plain
table per sheet. Every cell value is rendered as a *string* — no styles,
number/date formats, merged cells, drawings, or charts.

**Specs.** `[MS-XLS]` (the record stream, the SST, the cell records) and
`[MS-CFB]` for the container. Section numbers are cited inline below and in code.

---

## What works

- `.xls` is detected (`/Workbook` stream) and decoded to a `Document`
  (spreadsheet): one `sheet` element per worksheet, with `sheet_cell` →
  `paragraph` → `text` elements for every non-empty cell.
- **All BIFF8 cell value kinds** become display text: SST strings (`LabelSst`),
  inline strings (`Label`), numbers (`RK`, `MulRk`, `Number`), booleans/errors
  (`BoolErr`), and **cached formula results** (`Formula` + `String` for string
  results; numeric/boolean/error results from the `FormulaValue`).
- **SST `CONTINUE` splitting** is handled, including a split *mid-string* where
  the continuation re-declares the character encoding (§2.5.293).
- Sheet `dimensions` come from the `Dimensions` record; `content` is the tight
  extent of the non-empty cells (what the HTML renderer uses by default).
- The generic HTML renderer produces one table per sheet
  (`html::translate_sheet`), with column letters and row numbers.

Verified against `[MS-XLS]`: the record stream (§2.1.4), BOF/substream layout
(§2.4.21), `BoundSheet8` (§2.4.28), `SST`/`Continue` (§2.4.265/.58),
`XLUnicodeRichExtendedString` (§2.5.293), `RkNumber` (§2.5.217: bit 0 = `fX100`,
bit 1 = `fInt`), `FormulaValue` (§2.5.133), `Dimensions` (§2.4.90).

## Module layout (sibling of `../text`, `../presentation`)

| File (`oldms/spreadsheet/`)        | Role                                              |
|------------------------------------|---------------------------------------------------|
| `xls_structs.hpp`                  | `#pragma pack(1)` PODs for the record bodies + `static_assert` sizes + record type enum |
| `xls_io.{hpp,cpp}`                 | `BiffReader` (record walker with transparent `CONTINUE` hopping; the `[MS-XLS]` string readers and `expect_bof` are methods), RK decoding, number formatting |
| `xls_parser.{hpp,cpp}`             | `parse_tree(registry, files)` → globals (BoundSheet8 + SST) then one pass per sheet substream |
| `xls_element_registry.{hpp,cpp}`   | Flat element store + `Sheet` (name, dimensions, cell position map) and `SheetCell` (position) payloads |
| `xls_document.{hpp,cpp}`           | `internal::Document` subclass + the `ElementAdapter` |

## Pipeline: how a `.xls` becomes the element tree

1. **Wiring.** `LegacyMicrosoftFile::parse_meta` detects the `/Workbook` stream
   → `FileType::legacy_excel_worksheets`, `DocumentType::spreadsheet`, and
   `document()` returns `spreadsheet::Document`.
2. **Globals substream.** `/Workbook` is a flat sequence of `(u16 type, u16
   size, body)` records. The first substream (after its `BOF`, which must
   declare BIFF8 = `vers 0x0600`) holds, per sheet, a `BoundSheet8` (name +
   absolute offset of the sheet's `BOF`; only `dt == worksheet` is kept) and the
   `SST` — all shared string constants, deduplicated.
3. **SST / CONTINUE.** A record body is capped at 8224 bytes; the SST payload
   spills into `Continue` records, and the split can fall *inside* a string.
   `BiffReader`'s body accessors hop into a following `CONTINUE` transparently
   (throwing if the next record is anything else); character data additionally
   re-reads a fresh flags byte at each hop, since the continuation re-declares
   compressed (1 byte/char) vs UTF-16 for the remainder. Formatting runs
   (`cRun`·4 bytes) and phonetic data (`cbExtRst` bytes) are read and skipped.
4. **Sheet substreams.** For each kept `BoundSheet8`, seek to its `BOF` and scan
   records until `EOF`: `Dimensions` → sheet extents; `LabelSst` / `Label` /
   `RK` / `MulRk` / `Number` / `BoolErr` → one cell each; `Formula` → the cached
   result in its `FormulaValue` (an Xnum double unless `fExprO == 0xFFFF`, then
   string/bool/error/blank — a string result follows in a `String` record,
   matched via a pending-cell marker). `Blank` / `MulBlank` carry no text and
   are ignored.
5. **Tree.** Each non-empty cell becomes `sheet_cell → paragraph → text` (the
   cell's rendered string). Cells hang off their sheet by `parent_id` only —
   they are *not* in the sibling chain (mirrors `ooxml/spreadsheet`); lookup goes
   through the sheet's `(column,row) → id` map, which also tracks the tight
   `content` extent.
6. **Render.** `html::translate_sheet` walks the sheet purely through the public
   `Sheet` / `SheetCell` API, which delegates to our adapter.

### Value formatting

- **RK numbers** (§2.5.217): low 2 bits are flags — bit 0 `fX100` (divide by
  100), bit 1 `fInt` (30-bit signed integer vs the *high 30 bits* of an IEEE
  double, rest zero).
- Numbers are formatted with `%.15g` (≈ Excel's "General": up to 15 significant
  digits, no trailing zeros, integers without a decimal point).
- Booleans → `TRUE`/`FALSE`; error codes (BErr, §2.5.10) → `#DIV/0!`, `#VALUE!`,
  `#REF!`, `#NAME?`, `#NUM!`, `#N/A`, `#NULL!`.
- Dates are **not** decoded: a date cell shows its raw serial number unless the
  file stored it as a string (number-format handling is open work).

## Adapters

`xls_document.cpp` implements the generic `ElementAdapter` plus `SheetAdapter` /
`SheetCellAdapter` / `ParagraphAdapter` / `TextAdapter`:
- `sheet_name` / `sheet_dimensions` → from the registry payload;
  `sheet_content(range)` → the tight content extent, clamped to `range`.
- `sheet_cell(col,row)` → map lookup, `null_element_id` for empties;
  `sheet_first_shape` → none.
- All `*_style(...)` → `{}`; `sheet_cell_value_type` → `ValueType::string`
  (every value is pre-rendered text); `sheet_cell_span` → `{1,1}`.
- `paragraph_text_style` / `text_style` set `font_size = 11pt` so empty
  paragraphs have height (same hack as the `.doc`/`.ppt` modules).
- `Document::is_editable()` → `false`; `save(...)` → `UnsupportedOperation`.

## Design decisions

- **Fail early on malformed input** (matches the sibling modules): missing or
  non-BIFF8 `BOF`, a non-`CONTINUE` record where a body continuation is
  required, an out-of-range SST index, a malformed `MulRk` body, an unknown
  `FormulaValue` type, and truncated streams all **throw**. Records that are
  merely *not modelled* are skipped.
- **Pre-rendered text instead of typed values.** Cell values are converted to
  display strings at parse time; the model exposes `ValueType::string` only.
  Typed values would require XF/number-format plumbing — deliberately deferred.
- **Endianness/bit order**: bytes are copied straight into native
  integers/doubles and bit-field structs (`RkNumber`, `UnicodeStringFlags`, flag
  fields of `BoundSheet8Fixed`/`FormulaFixed`) — little-endian, LSB-first hosts
  only; shared `oldms/` assumption, see [`../AGENTS.md`](../AGENTS.md).

## Tests

- `xls_string_split_across_continue` — a string split mid-character-data with an
  encoding switch at the boundary.
- `xls_rich_string_runs_across_continue` — formatting-run skip across a
  `CONTINUE` (no flags byte there) + correct position for the next string.
- `xls_decode_rk` — all four RK flag combinations + number formatting; the
  inputs are raw on-disk encodings, so it also pins the `RkNumber` bit-field
  layout.
- `xls_empty` / `xls_file_example_10` / `xls_file_example_5000` — real fixtures:
  sheet names, dimensions, content extents, string/number cells; the 5000-row
  file exercises SST `CONTINUE` handling on real data.
- HTML output: `html_output_test` no longer skips `legacy_excel_worksheets`;
  reference output lives under
  `test/data/reference-output/{odr-public,odr-private}/output/xls/`.

---

# Open work

Roughly ordered by value.

## 1. Number & date formatting (the biggest visible gap)

Cells currently show raw values: a date cell renders as its serial number (e.g.
`43023` instead of `15/10/2017`) and numbers ignore their format codes. Fix by
following the format chain:
- Each cell record carries an `ixfe` (currently discarded — the parser already
  reads it). It indexes the `XF` records (0x00E0) in the globals substream;
  `XF.ifmt` picks a number format: a built-in id (0–163, the table is in
  [MS-XLS] 2.4.126 `Format`) or a `Format` record (0x041E) with a format string.
- MVP: keep `ixfe` per cell, parse `XF`/`Format`, and special-case the date/time
  formats (built-in ids 14–22, 45–47 + anything containing `y/m/d/h`) to convert
  the serial date (days since 1899-12-31, fractional part = time; mind the
  workbook's 1904 flag in `Date1904`, 0x0022) into a sensible string. Full
  custom-format rendering is a rabbit hole; approximate first.

## 2. Coverage gaps

- **Merged cells**: `MergeCells` record (0x00E5) → `sheet_cell_span` /
  `sheet_cell_is_covered` (the adapter stubs are in place).
- **Styles**: fonts (`Font`, 0x0031), fills/borders from `XF` →
  `sheet_cell_style` / `text_style`; column widths (`ColInfo`, 0x007D) and row
  heights (`Row`, 0x0208) → `sheet_column_style` / `sheet_row_style`.
- **Hidden rows/columns** (`Row.fDyZero`, `ColInfo.fHidden`).
- **Typed cell values**: expose numeric/bool/date `ValueType`s instead of
  pre-rendered strings (needed for anything smarter than HTML text).
- **Encrypted workbooks**: a `FilePass` record (0x002F) in the globals substream
  means the rest of the stream is encrypted ([MS-OFFCRYPTO]) — currently it
  parses as garbage or throws; should report password-protected.
- **BIFF5/BIFF7** (`BOF.vers != 0x0600`): currently throws; older `.xls` files
  exist in the wild (no SST — `Label` records carry the strings inline).
- **Drawings/charts/images** (`MsoDrawing`/`Obj`/chart substreams) — likely
  never worth it for text extraction.

## 3. Smaller shortcomings

- **Endianness/bit order** — shared `oldms/` shortcoming, see
  [`../AGENTS.md`](../AGENTS.md).
- `RString` (0x00D6, rich inline string cell) is rare and currently skipped.
- A `Formula` string result is matched to the *immediately following* `String`
  record via a pending-cell marker; an intervening `SharedFmla`/`Array`/`Table`
  record is tolerated only because unknown records are skipped — not validated.
