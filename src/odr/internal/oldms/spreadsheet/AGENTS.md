# `.xls` (Excel / BIFF8) support: design & open work

Shared `oldms/` conventions are in [`../AGENTS.md`](../AGENTS.md).

**Scope.** The visible cell text of every worksheet, plus each cell's font
formatting and fill color, so the generic HTML renderer produces a styled
table per sheet. Every cell value is a string. No number or date formats,
merged cells, borders, drawings or charts.

**Specs.** `[MS-XLS]` (record stream, SST, cell records, Font, XF, Palette)
and the `[MS-CFB]` container.

## Module layout

| File (`oldms/spreadsheet/`) | Role |
|---|---|
| `xls_structs.hpp` | `#pragma pack(1)` PODs for record bodies, the record-type enum |
| `xls_io.{hpp,cpp}` | `BiffReader` (record walker with transparent `CONTINUE` hopping, the `[MS-XLS]` string readers, `expect_bof`), RK decoding, `format_number`, `error_code_string` |
| `xls_parser.{hpp,cpp}` | `parse_tree`: the globals (BoundSheet8, SST, Font, XF, Palette), then one pass per sheet substream |
| `xls_element_registry.{hpp,cpp}` | The element store plus `Sheet` (name, dimensions, cell-position map) and `SheetCell` payloads |
| `xls_style.{hpp,cpp}` | `StyleRegistry`: resolves Font, XF and Palette into one `ResolvedStyle` per XF, indexed by a cell's `ixfe` |
| `xls_document.{hpp,cpp}` | `internal::Document` subclass and the `ElementAdapter` |

The tree is `sheet → sheet_cell → paragraph → text`, one `sheet_cell` per
non-empty cell.

## Design decisions

**Pre-rendered text instead of typed values.** The parser converts every
value to its display string, and the model exposes `ValueType::string` only.
SST strings (`LabelSst`), inline strings (`Label`), numbers (`RK`, `MulRk`,
`Number`), booleans and errors (`BoolErr`) and cached formula results
(`Formula`, with a following `String` record for a string result, matched
through a pending-cell marker) all become text. Typed values need the
number-format chain (open work §1).

**Cells hang off their sheet by `parent_id` only.** They are not in the
sibling chain (as in `ooxml/spreadsheet`). Lookup goes through the sheet's
`(column, row)` to id map, which also tracks the tight content extent the
renderer uses. `sheet_dimensions` comes from the `Dimensions` record.

**SST `CONTINUE` splitting.** A record body is capped at 8224 bytes, so the
SST spills into `Continue` records, and the split can fall inside a string.
`BiffReader`'s body accessors hop into a following `CONTINUE` and throw if
the next record is anything else. Character data re-reads a flags byte at
each hop, because the continuation re-declares compressed versus UTF-16 for
the remainder (§2.5.293). Formatting runs (`cRun`·4 B) and phonetic data
(`cbExtRst` B) are read and skipped.

**Fail early.** Throw on: a missing or non-BIFF8 `BOF` (`vers != 0x0600`); a
non-`CONTINUE` record where a continuation is required; an out-of-range SST
index; a malformed `MulRk` body; an unknown `FormulaValue` type; a truncated
stream. Skip records that are not modelled.

**Cell formatting is resolved at parse time, per XF, in the `StyleRegistry`.**
The parser fills both registries, because BIFF keeps styles and content in
the same `/Workbook` stream. Each cell keeps its `ixfe` on the `SheetCell`.
The globals pass collects `Font` (0x0031), `XF` (0x00E0) and `Palette`
(0x0092), and the `StyleRegistry` constructor resolves every XF into a
`ResolvedStyle` (a `TextStyle` from the font and a `TableCellStyle` fill).
The adapters look up: `text_style` and `paragraph_text_style` walk up to the
`sheet_cell` ancestor and return its XF's `TextStyle`; `sheet_cell_style`
returns the fill.

- `FontIndex` 4 does not exist (§2.5.129). `ifnt` below 4 is zero-based,
  above 4 is one-based into the Font records in file order.
- Colors are `Icv` indexes (§2.5.161): 0x00 to 0x07 are built-in constants,
  0x08 to 0x3F index the `Palette` record (or the spec's default palette when
  absent), 0x40, 0x41 and 0x7FFF are system or automatic and stay unset.
- Fills (§2.5.20): `fls` 0 is none, solid (1) renders `icvFore`, every other
  pattern is approximated by its foreground color.
- A `dyHeight` of 0 (allowed by §2.4.122) leaves `font_size` unset. The
  `StyleRegistry` owns the parsed `Font` records, so `TextStyle::font_name`
  (`const char *`) points into them and stays valid.

**Adapters** expose `ValueType::string` for every cell, `sheet_cell_span` of
`{1,1}` and `sheet_cell_is_covered` false. Sheet, column and row styles are
`{}`. `Document::is_editable()` is `false`. `save` throws.

**Value formatting.**

- RK numbers (§2.5.217): the low 2 bits are flags, bit 0 `fX100` (divide by
  100), bit 1 `fInt` (a 30-bit signed int, else the high 30 bits of an IEEE
  double).
- Numbers use `%.15g`, close to Excel's "General". Booleans are `TRUE` and
  `FALSE`. Errors (§2.5.10) are `#DIV/0!`, `#VALUE!`, `#REF!`, `#NAME?`,
  `#NUM!`, `#N/A` and `#NULL!`.
- A date cell shows its raw serial number (open work §1).

## Tests

- `OldMs.xls_string_split_across_continue`: a split inside character data
  with an encoding switch at the boundary.
- `OldMs.xls_rich_string_runs_across_continue`: the formatting-run skip
  across `CONTINUE` and the next-string position.
- `OldMs.xls_decode_rk`: all four RK flag combinations, on raw encodings, so
  it also pins the `RkNumber` bit-field layout.
- `OldMs.xls_cell_styles`: a synthetic workbook: XF to Font resolution
  including the skipped index 4, weight, italic, underline, strike, the
  default palette, automatic colors, a solid fill, unstyled empty positions.
- `OldMs.xls_palette_record`: a `Palette` record that overrides the default
  palette.
- `OldMs.xls_empty`, `OldMs.xls_file_example_10`,
  `OldMs.xls_file_example_5000`: real fixtures. The 5000-row file exercises
  SST `CONTINUE` on real data.
- `OldMsEncryption.*`: the `FilePass` record.
- `html_output_test` compares the `.xls` fixtures against the reference output.

# Open work

## 1. Number and date formatting

A date cell renders as its serial number, and numbers ignore their format
codes. Each cell's `ixfe` (kept on the `SheetCell`) indexes the `XF` records,
and `XF.ifmt` picks a number format: a built-in id (0 to 163, table in
[MS-XLS] 2.4.126) or a `Format` record (0x041E) string. A first step: parse
`XF.ifmt` and `Format`, special-case the date and time formats (built-in ids
14 to 22, 45 to 47, and anything with `y`, `m`, `d` or `h`), and convert the
serial (days since 1899-12-31, fraction is the time, `Date1904` 0x0022 moves
the epoch). Full custom-format rendering comes later.

## 2. Coverage gaps

- Merged cells: `MergeCells` (0x00E5) into `sheet_cell_span` and
  `sheet_cell_is_covered`.
- Remaining styles: borders and alignment from `XF` (parsed, unused), column
  widths (`ColInfo` 0x007D), row heights (`Row` 0x0208). `Blank` and
  `MulBlank` cells are skipped, so a fill on an empty cell is lost.
- Hidden rows and columns (`Row.fDyZero`, `ColInfo.fHidden`).
- Typed cell values: numeric, boolean and date `ValueType`s instead of
  strings.
- Decryption ([MS-OFFCRYPTO]). See [`../AGENTS.md`](../AGENTS.md).
- BIFF5 and BIFF7 (`vers != 0x0600`) throw. Older files have no SST, and
  `Label` records carry strings inline.
- Drawings, charts and images.

## 3. Smaller shortcomings

- Endianness. See [`../AGENTS.md`](../AGENTS.md).
- `RString` (0x00D6, a rich inline string cell) is skipped.
- A `Formula` string result is matched to the next `String` record. An
  intervening `SharedFmla`, `Array` or `Table` is tolerated only because
  unknown records are skipped.
