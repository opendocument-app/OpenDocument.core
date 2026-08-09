# CSV plan

Where the csv module is going, and in what order. Written before stage 1; keep
it honest as stages land.

## Today

`CsvFile` is detection only — 60 lines, `is_decodable() == false`, rendered by
`html::create_text_service` as a line list. `check_csv_file` reads the **whole**
file, hard-codes `,` and `"`, and demands a globally uniform field count > 1.
`text::TextFile`'s constructor runs `guess_charset` over the **whole** stream,
and `open_strategy` does both speculatively while listing file types.

The detected charset is currently used nowhere: `html/text_file.cpp` pipes raw
bytes into a document declaring `<meta charset="UTF-8">`, so any non-UTF-8 text
file already renders as mojibake (`// TODO charset`, `html/text_file.cpp:71`).

## Target

A csv opens as a spreadsheet document — one sheet, cells reachable through
`SheetAdapter` — so the generic HTML renderer and every binding get table
rendering without format-specific code. Separator, charset, quoting and header
are autodetected from a bounded probe, overridable by the caller, and the
resolved values are readable back.

## Decisions taken up front

**Cell strings are UTF-8.** `Text::content()` returns `std::string` and every
binding treats it as UTF-8 (JNI `NewStringUTF`, embind, pybind11, `NSString`).
Passing legacy bytes through and letting a browser sort it out only works for
the html path, not the api path, so it is not an option. This is what makes an
encoding we cannot decode fatal to the *sheet* path specifically, while the
text path stays open to it.

**No element per cell.** `ElementIdentifier` is a `std::uint64_t`
(`definitions.hpp:7`): encode `kind | row | column` into it and decode in the
adapter. A flat `ElementRegistry` over cells does not survive a large file, and
the virtual-id seam is what keeps every later scaling decision an
implementation detail. This is a deliberate deviation from the pattern in the
root `AGENTS.md`; it belongs in a `csv/AGENTS.md` once stage 2 lands.

**Detection rejects; the parser does not.** These are two jobs and
`check_csv_file` currently conflates them — `open_strategy.cpp:281` even
constructs a `CsvFile` *as* the probe, which is why detection pays for a full
scan. Split into a probe returning resolved options plus a verdict, and a parser
taking resolved options and judging nothing.

Detection heuristics (sniffing an unknown text file): score candidate separators
over the probe, require ≥ 2 columns, reject an unterminated quote at EOF. Both
rules are discriminators — one column is every line of prose ever written, and a
dangling quote is good evidence of not-csv — not statements about what is valid.

The parser, given a separator, is **total**: any bytes plus a transcodable
charset yield some sheet. A one-column csv is a legitimate csv; a truncated
quoted field closes at EOF; an empty file is an empty sheet; ragged rows pad
short and widen long. Nothing here is unrepresentable, so nothing throws.

What is left to fail is an incoherent options struct (separator equal to the
quote char, a separator of `"` or `\n`) — an argument error — and a charset we
cannot transcode. `NoCsvFile` therefore becomes purely a detection exception,
which is what its name always claimed.

**Quoting is not type information.** RFC 4180 quoting is lexical — Excel quotes
any field holding a delimiter, quote or newline, and many writers quote
everything. Infer nothing from it.

**Encoding is a public enum with a table behind it.** Encodings carry aliases
the way mime types do (`UTF-8`/`utf8`, `windows-1252`/`cp1252`,
`ISO-8859-1`/`latin1`) and uchardet hands back a name that has to map home, so
unlike `FileType` this needs both directions. Mirror `file_type_table.cpp`:
canonical name first, aliases after, one row per encoding, and a test that
fails when an alias is claimed twice.

```cpp
enum class TextEncoding { unknown, utf8, utf16le, utf16be, utf32le, utf32be,
                          windows_1252, iso_8859_1, iso_8859_15, shift_jis, … };

std::vector<TextEncoding> all_text_encodings();
std::string text_encoding_to_string(TextEncoding);
TextEncoding text_encoding_by_name(std::string_view) noexcept;  // unknown if none
std::span<const std::string_view> text_encoding_names(TextEncoding) noexcept;
bool text_encoding_is_decodable(TextEncoding) noexcept;
```

Named `TextEncoding`/`encoding()` rather than `Charset`/`charset()` so it does
not collide with the accessor it replaces. `TextFile::charset()` stays,
implemented as `text_encoding_to_string(encoding())` and marked both
`/// @deprecated` (the existing convention, `html.hpp:49`) and `[[deprecated]]`.
There is no `-Werror` (`CMakeLists.txt:38`), so the attribute is warnings only;
the three binding call sites (`jni_file.cpp:288`, `bind_file.cpp:218`,
`ODRFile.mm:516`) move to `text_encoding_to_string(encoding())` and keep their
own public surface unchanged.

**Reuse from `pdf` means extraction first.** Nothing reaches across into
`internal/pdf`. Anything shared moves into `internal/encoding` with pdf as one
of its users. Stage 1 as designed needs no runtime reuse at all, so this binds
only on the multibyte tier below.

**Encoding tiers**, expressed as that last predicate.

- **UTF-8/16/32 (+BOM)** — `utfcpp`, already a dep.
- **Single-byte legacy** (windows-1252, ISO-8859-1/-15) — a generated 256-entry
  byte→codepoint table per encoding. Generate from Python's built-in codecs
  (`bytes([i]).decode('cp1252')`), which are the standard mappings; only the
  *shape* comes from `tools/pdf/generate_encoding_data.py` → `pdf_encoding_data.cpp`
  (auto-generated header, `clang-format off`, licence note). Do **not** reuse
  WinAnsiEncoding as cp1252: they are close, not equal, and ISO-8859-x is not
  in the pdf tables at all — the header on `pdf_doc_encoding_to_unicode`
  documents exactly this class of near-miss.
- **Multibyte legacy** (Shift-JIS/cp932, GBK, Big5, EUC-KR) — *deferred, not
  declined*, with two viable routes and no need to pick until someone wants CJK
  csv. Measured mapping counts: cp932 22 736, gbk 21 791, shift_jis 18 912,
  big5 13 710, euc_kr 8 225.
  - *Generate our own*, again from Python's codecs: exact, no refactor, but
    ~85 k pairs ≈ 340 KB of static data as sorted `(uint16, char16)`, less with
    the run encoding `pdf_cid_data` uses.
  - *Hoist and share*: `pdf_cid.hpp:19` documents `translate_predefined_cmap`
    as decoding the legacy CJK CMaps, "whose codes are the legacy encoding's
    bytes", to UTF-8, and `pdf_cid_data.cpp` carries 17 RKSJ CMaps —
    `90ms-RKSJ-H` being the Microsoft variant, i.e. cp932. That data is already
    compiled into every binary, so sharing it costs no size. The price is the
    extraction into `internal/encoding` and the lossiness of routing through a
    character collection's UCS2 map.

Until then a non-decodable encoding is *known but not decoded*: reported by
name, rendered as text with `text_encoding_to_string(...)` in the html header so
the browser decodes it. Only the spreadsheet path, which must hand UTF-8 to the
bindings, is closed to it.

---

## Stage 1 — encoding, in `internal/encoding`

No csv in this stage at all. It is the substrate everything else stands on, it
ships alone, and it closes a live bug: the detected charset is used nowhere
today, so every non-UTF-8 text file renders as mojibake. A package of its own
rather than a corner of `internal/text`, because it is what `pdf` would later
share.

- public `TextEncoding` enum + `text_encoding_table.cpp` (aliases, both
  directions, decodable predicate) as above; `TextFile::encoding()` added,
  `charset()` deprecated, bindings moved off it.
- `to_utf8(std::string_view, TextEncoding)`. BOM sniffing is deterministic and
  runs before uchardet. Single-byte tables generated from Python's codecs, in
  the `tools/pdf` output shape. Invalid bytes become U+FFFD, never a throw
  mid-render — the override is the answer to a bad guess.
- `guess_charset(std::istream &, std::size_t max_bytes)` — honour the existing
  TODO in `text_util.hpp`. 64 KiB is enough for uchardet. Accept the
  consequence: a file that is ASCII for 64 KiB with a `é` at 2 MB detects as
  UTF-8 and meets an invalid byte later. That is what U+FFFD and the override
  are for.
- a binary test (NUL byte / share of C0 controls) as the actual "is this text"
  oracle. uchardet names a charset, it does not gate.
- `html/text_file.cpp` transcodes, or declares the charset for a non-
  transcodable one, and drops its `// TODO charset`.

## Stage 2 — the csv probe

Splits detection from parsing and stops detection paying for a full scan.

- `probe` reads one bounded prefix, shared by charset detection and separator
  scoring, and returns resolved options plus a verdict. Drop the trailing
  partial record from the sample.
- separator scoring over `{',', ';', '\t', '|'}`: quote-aware split, modal field
  count per candidate, pick by share-of-lines-matching then field count; a
  winner yielding one column loses. Honour Excel's `sep=;` first line.
- `open_strategy` keeps constructing a `CsvFile` to probe, because that
  constructor *is* the probe now and is bounded. Detection reads its own 64 KiB
  rather than sharing `text::TextFile`'s: caching the probe would make every
  text file carry 64 KiB for the life of the object, and re-reading it costs
  nothing.
- the existing tests in `csv_file_test.cpp` move to the probe: ragged rows and
  dangling quotes stay rejections *there*, and stop being rejections in the
  parser. The comment on `a_quoted_field_must_be_terminated` records a real
  misclassification — the rule survives, on the detection side only.

## Stage 3 — options, shaped like decryption

`nullopt` means autodetect; the resolved values are readable back so a caller
can show "detected `;`, UTF-16LE" and offer an override. The realistic flow is
open → wrong → adjust → reopen.

```cpp
struct CsvOptions {
  std::optional<Charset> charset;
  std::optional<char> separator;
  std::optional<char> quote;
  std::optional<bool> header_row;
  std::size_t probe_bytes{1 << 16};
};
```

Follow `decrypt`: an immutable handle deriving another handle, narrowed on the
concrete type (`file.cpp:332`, `file.cpp:345`). `TextFile::charset()` is already
half of this pattern — stage 3 finishes it.

```cpp
CsvOptions options() const;
CsvFile with_options(const CsvOptions &) const;
```

This keeps `odr::open` untouched — no new overload across six `open` signatures
and six `DecodedFile` constructors — and the bindings already have the call
pattern from decrypt (`wasm/src/wasm_file.cpp:86`, `jni/src/jni_file.cpp:162`).
Do **not** copy `decrypt`'s state machine: `EncryptionState` gates rendering
because an encrypted file cannot be rendered at all (`file.cpp:167`), whereas a
csv without options always has a guess. An `options_required()` state would
invent a gate the format does not have. Nor does the failure map: a wrong
password leaves nothing to hand back, whereas the parser is total, so
`with_options` fails only on an incoherent struct or an untranscodable charset.

Options are per-format, so they need a concrete handle, but stage 2 routes csv
through `as_document_file()` which has nowhere to put them. Add a public
`CsvFile` handle alongside `TextFile`/`PdfFile` — `as_csv_file()`, `options()`,
`with_options()`, `document()`. `PdfFile` is the precedent for a format-specific
handle narrowing an inherited operation. The same shape gives `TextFile` a
`with_charset(std::string)` next to its existing `charset()`.

`header_row` is not in the struct: it would be inert until a sheet exists to
mark a header on. It arrives with stage 4, or not at all.

**Bindings come after stage 4, not here.** `CsvFile` is the handle stage 4 hangs
`document()` off, and stage 4 also decides the file category — binding it now
means binding it twice. One binding pass, once the shape has settled.

## Stage 4 — the sheet document

The visible win. Whole file in memory; the renderer caps at
`spreadsheet_limit{10000, 500}` (`html.hpp:124`) so that is sufficient for
output at any file size worth caring about today.

- `CsvDocument` : `internal::Document`, `DocumentType::spreadsheet`, one sheet.
- virtual element ids as decided above; `cell`/`dimensions` are the only way in
  to the data, so stage 6 can change what is behind them.
- the sheet is rectangular even where the file is not — a short row pads, a
  long one widens. The counterpart to detection no longer rejecting ragged
  files.
- csv **stays** `FileCategory::text` (`file_type_table.cpp:416`), carrying
  `DocumentType::spreadsheet`. Moving it to `document` was the plan and is not
  what landed: a csv is still text, `TextFile::text()` still reads it, and
  `document()` is the second view rather than a replacement. So `is_text_file()`
  keeps answering true and `html::translate` orders its csv branch first. The
  capabilities test enforces whatever the row claims.
- `html_output_test` skips csv (`// TODO enable zip, csv, json`). Csv now
  produces real output, so enabling it needs reference output committed to the
  output repo and the pointer advanced — a separate repo, a separate change.

## Stage 5 — value types

`ValueType` is only `{unknown, string, float_number}`
(`document_element.hpp:135`) and drives one CSS class — right alignment — so it
stays small. Decided per *column*: a reader compares down a column, so one
number in a column of prose is not a quantity. The first row is left out of the
sample, because a header names its column rather than holding a value.

The grammar is strict on purpose. A leading zero is what tells `007` from a
quantity, and a thousands separator does not say which side of the Atlantic
wrote it — `1,234` is a thousand or one point two three four depending. Dates
are left alone entirely: `03/04/2026` is two different days and guessing gets
it wrong confidently. The raw string is always what is displayed.

## Stage 6 — large files

Deferred: with the 10 000-row render cap this buys nothing for html output. It
matters for open cost and for api consumers walking the sheet. Build it when
something needs it; `CsvDocument::cell`/`dimensions` are the seam.

- Row-start offsets are the only index needed — a row boundary is the one place
  where "outside quotes" is unambiguous.
- Checkpoint every ~1024 rows (offset + row index), binary search then scan
  forward. 10 M rows at 8 bytes each is 80 MB and unshippable on a phone;
  checkpoints are ~80 KB per GB.
- `sheet_dimensions()` needs an exact count, i.e. one full quote-aware scan.
  Make it lazy — first dimension query, not open — and build the checkpoints in
  that pass.
- Window staging: unescape **into** the staged buffer. `""` means a cell's value
  is not a contiguous range of the original bytes, but unescaping only shrinks,
  so each field compacts within its own slot and a `string_view` into the
  rewritten window is valid. The window also holds the transcoded UTF-8, so one
  buffer solves both. LRU 2–4 windows; the renderer's row-major walk hits them
  sequentially.
- `abstract::File` offers `memory_data()` (whole file as a `string_view`, no
  staging needed) and `disk_path()` (mmap-able). A csv inside a zip has neither
  — that path stays full in-memory, by decision not by accident.
