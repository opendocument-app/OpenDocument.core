# `.xls` (Excel / BIFF8) support — design & open work

The **why** and the roadmap; what is concretely done lives in the code. Shared
`oldms/` conventions are in [`../AGENTS.md`](../AGENTS.md).

**Scope.** Extract the **visible cell text** of every worksheet, plus each
cell's **font formatting and fill color**, through the abstract model so the
generic HTML renderer produces a styled table per sheet. Every cell value is
rendered as a *string* — no number/date formats, merged cells, borders,
drawings, or charts.

**Specs.** `[MS-XLS]` (record stream, SST, cell records, Font/XF/Palette) +
`[MS-CFB]` container. Section numbers cited inline in code.

## Module layout (sibling of `../text`, `../presentation`)

| File (`oldms/spreadsheet/`) | Role |
|---|---|
| `xls_structs.hpp` | `#pragma pack(1)` PODs for record bodies + `static_assert` sizes + record-type enum |
| `xls_io.{hpp,cpp}` | `BiffReader` (record walker with transparent `CONTINUE` hopping; the `[MS-XLS]` string readers + `expect_bof`), RK decoding, number formatting |
| `xls_parser.{hpp,cpp}` | `parse_tree` → globals (BoundSheet8 + SST + Font/XF/Palette) then one pass per sheet substream |
| `xls_element_registry.{hpp,cpp}` | Flat element store + `Sheet` (name, dimensions, cell-position map) and `SheetCell` payloads |
| `xls_style.{hpp,cpp}` | `StyleRegistry`: resolves Font/XF/Palette into one `ResolvedStyle` per XF, indexed by a cell's `ixfe` (sibling of the odf/ooxml style registries) |
| `xls_document.{hpp,cpp}` | `internal::Document` subclass + the `ElementAdapter` |

Tree shape: `sheet → sheet_cell → paragraph → text`, one `sheet_cell` per
non-empty cell.

## Design decisions

**Pre-rendered text instead of typed values.** Cell values are converted to
display strings at parse time; the model exposes `ValueType::string` only. All
BIFF8 value kinds become text: SST strings (`LabelSst`), inline (`Label`), numbers
(`RK`/`MulRk`/`Number`), booleans/errors (`BoolErr`), and cached formula results
(`Formula` + a following `String` record for string results, matched via a
pending-cell marker). Typed values would require XF/number-format plumbing —
deliberately deferred (open work §1).

**Cells hang off their sheet by `parent_id` only** — they are *not* in the sibling
chain (mirrors `ooxml/spreadsheet`); lookup goes through the sheet's `(column,row)
→ id` map, which also tracks the tight `content` extent (what the HTML renderer
uses by default). `sheet_dimensions` comes from the `Dimensions` record.

**SST `CONTINUE` splitting is the subtle part.** A record body is capped at 8224
bytes, so the SST spills into `Continue` records and the split can fall *inside* a
string. `BiffReader`'s body accessors hop into a following `CONTINUE` transparently
(throwing if the next record is anything else); character data additionally
re-reads a fresh flags byte at each hop, since the continuation re-declares
compressed (1 B/char) vs UTF-16 for the remainder (§2.5.293). Formatting runs
(`cRun`·4 B) and phonetic data (`cbExtRst` B) are read and skipped.

**Fail early on malformed input** (matches the siblings): missing/non-BIFF8 `BOF`
(`vers != 0x0600`), a non-`CONTINUE` record where a body continuation is required,
an out-of-range SST index, a malformed `MulRk` body, an unknown `FormulaValue`
type, and truncated streams all **throw**. Records merely *not modelled* are
skipped.

**Endianness/bit order.** Bytes copied straight into native integers/doubles and
bit-field structs (`RkNumber`, `UnicodeStringFlags`, flags of
`BoundSheet8Fixed`/`FormulaFixed`) — little-endian, LSB-first hosts only; shared
`oldms/` assumption, see [`../AGENTS.md`](../AGENTS.md).

**Cell formatting is resolved at parse time, per XF, in a separate
`StyleRegistry`** (mirroring the odf/ooxml split between element and style
registries; here the parser fills both, since BIFF keeps styles and content in
the same `/Workbook` stream). Each cell record's `ixfe` is kept on the
`SheetCell`; the globals pass collects `Font` (0x0031), `XF` (0x00E0) and
`Palette` (0x0092), and the `StyleRegistry` constructor resolves every XF into
a `ResolvedStyle` (a `TextStyle` from the font + a `TableCellStyle` fill),
indexed by `ixfe`. The adapters only look up: `text_style`/
`paragraph_text_style` walk up to the `sheet_cell` ancestor and return its
XF's `TextStyle`; `sheet_cell_style` returns the fill. The non-obvious bits:
- **`FontIndex` 4 does not exist** (§2.5.129): `ifnt` < 4 is zero-based,
  \> 4 is one-based into the Font records in file order.
- **Colors are `Icv` indexes** (§2.5.161): 0x00–0x07 built-in constants,
  0x08–0x3F the `Palette` record (or the spec's default palette when absent),
  0x40/0x41/0x7FFF system/automatic → left unset.
- **Fills** (§2.5.20): `fls` 0 = none; solid (1) renders `icvFore`; the other
  patterns are *approximated* by their foreground color.
- A `dyHeight` of 0 (allowed by §2.4.122) leaves `font_size` unset instead of
  emitting invisible 0pt text. The `StyleRegistry` owns the parsed `Font`
  records, so `TextStyle::font_name` (`const char *`) points into them and
  stays valid.

**Adapters** expose `ValueType::string` for every cell and `sheet_cell_span` →
`{1,1}`; sheet/column/row styles are still `{}`.
`Document::is_editable()` → `false`; `save` throws.

### Value formatting (the non-obvious bits)

- **RK numbers** (§2.5.217): low 2 bits are flags — bit 0 `fX100` (÷100), bit 1
  `fInt` (30-bit signed int vs the *high 30 bits* of an IEEE double, rest zero).
- Numbers use `%.15g` (≈ Excel "General"). Booleans → `TRUE`/`FALSE`; errors
  (§2.5.10) → `#DIV/0!`, `#VALUE!`, `#REF!`, `#NAME?`, `#NUM!`, `#N/A`, `#NULL!`.
- **Dates are not decoded**: a date cell shows its raw serial number unless the
  file stored it as a string (open work §1).

## Tests

- `xls_string_split_across_continue` — split mid-character-data with an encoding
  switch at the boundary.
- `xls_rich_string_runs_across_continue` — formatting-run skip across `CONTINUE`
  (no flags byte there) + correct next-string position.
- `xls_decode_rk` — all four RK flag combinations + number formatting (raw on-disk
  encodings, so it also pins the `RkNumber` bit-field layout).
- `xls_cell_styles` — synthetic one-sheet workbook (inline bytes): XF → Font
  resolution incl. the skipped index 4, weight/italic/underline/strike, the
  default palette, automatic colors, solid fill, unstyled empty positions.
- `xls_palette_record` — a `Palette` record overriding the default palette.
- `xls_empty` / `xls_file_example_10` / `xls_file_example_5000` — real fixtures
  (names, dimensions, extents, string/number cells; the 5000-row file exercises
  SST `CONTINUE` on real data).
- HTML output wired (`html_output_test` no longer skips `legacy_excel_worksheets`;
  reference under `test/data/reference-output/…/output/xls/`).

---

# Open work

Roughly ordered by value.

## 1. Number & date formatting (the biggest visible gap)

A date cell renders as its serial number (e.g. `43023` not `15/10/2017`); numbers
ignore their format codes. Fix by following the format chain:
- Each cell carries an `ixfe` (already read, currently discarded) indexing the
  `XF` records (0x00E0) in globals; `XF.ifmt` picks a number format: a built-in id
  (0–163, table in [MS-XLS] 2.4.126) or a `Format` record (0x041E) string.
- MVP: keep `ixfe` per cell, parse `XF`/`Format`, special-case the date/time
  formats (built-in ids 14–22, 45–47 + anything with `y/m/d/h`) to convert the
  serial date (days since 1899-12-31, fractional = time; mind the workbook 1904
  flag in `Date1904` 0x0022). Full custom-format rendering is a rabbit hole;
  approximate first.

## 2. Coverage gaps

- **Merged cells**: `MergeCells` (0x00E5) → `sheet_cell_span`/
  `sheet_cell_is_covered` (adapter stubs in place).
- **Remaining styles**: borders and alignment from `XF` (fields already
  parsed, unused); column widths (`ColInfo` 0x007D) and row heights (`Row`
  0x0208). `Blank`/`MulBlank` cells are skipped entirely, so a fill on an
  empty cell is lost. Non-solid fill patterns render as their foreground
  color instead of a pattern.
- **Hidden rows/columns** (`Row.fDyZero`, `ColInfo.fHidden`).
- **Typed cell values**: expose numeric/bool/date `ValueType`s instead of
  pre-rendered strings.
- **Encrypted workbooks**: a `FilePass` (0x002F) in globals means the rest is
  encrypted ([MS-OFFCRYPTO]); currently parses as garbage or throws — should report
  password-protected.
- **BIFF5/BIFF7** (`vers != 0x0600`): currently throws; older files exist in the
  wild (no SST — `Label` records carry strings inline).
- **Drawings/charts/images** — likely never worth it for text extraction.

## 3. Smaller shortcomings

- **Endianness/bit order** — shared `oldms/` shortcoming, see
  [`../AGENTS.md`](../AGENTS.md).
- `RString` (0x00D6, rich inline string cell) is rare and skipped.
- A `Formula` string result is matched to the *immediately following* `String`
  record; an intervening `SharedFmla`/`Array`/`Table` is tolerated only because
  unknown records are skipped — not validated.
