# `.doc` (Word) support — status, design & open work

What the `oldms/text/` module does **today**, the **design decisions** behind
it, and the **open work**. Shared `oldms/` conventions are in
[`../AGENTS.md`](../AGENTS.md).

**Scope.** Extract the **visible text of the main document body**, split into
paragraphs and manual line breaks, and expose it through the abstract document
model so the generic HTML renderer lays it out as a flat run of paragraphs. No
character/paragraph styles, no headers/footers/footnotes/endnotes/annotations,
no tables, frames, images, or fields beyond showing their result text.

**Specs.** `[MS-DOC]` (the FIB, the Clx / piece table, text decoding) and
`[MS-CFB]` for the container. Section numbers are cited inline below.

---

## What works

- `.doc` is detected (`/WordDocument` stream) and decoded to a `Document`
  (text), one flat element tree under the root.
- The **main document body** (the first `ccpText` characters) is read from the
  piece table, decoded (compressed 8-bit *or* UTF-16), split into paragraphs /
  manual line breaks, with a `page_break` element at each end-of-section /
  manual page break (`0x0C`).
- Field codes are resolved to their **result** text (the instruction part is
  hidden); anchor/control characters are stripped.
- The generic HTML renderer produces the body as a sequence of paragraphs.

Verified against `[MS-DOC]`: the read path matches *Retrieving Text* (§2.4.1,
steps 1–6), the FIB version map (§2.5.1), the Clx / Pcdt / Prc lead bytes
(§2.9.38/.178/.209), `FcCompressed` incl. the `0x82–0x9F` byte map (§2.9.73),
and the field characters (§2.8.25).

## Module layout (sibling of `../presentation`)

| File (`oldms/text/`)              | Role                                                |
|-----------------------------------|-----------------------------------------------------|
| `doc_structs.hpp`                 | `#pragma pack(1)` PODs (`FibBase`, the `FibRgFcLcb97/2000/2002/2003/2007` chain, `Sprm`, `FcCompressed`, `Pcd`) + `static_assert` sizes + the `PlcPcdMap` piece-table view + `ParsedFib` |
| `doc_io.{hpp,cpp}`                | `read(...)` helpers over `std::istream`: the variable-length FIB, the Clx walk, string decoding (compressed / UTF-16) |
| `doc_helper.{hpp,cpp}`            | `CharacterIndex` (the decoded piece table) + `read_character_index` |
| `doc_parser.{hpp,cpp}`           | `parse_tree(registry, files)` → reads the body text and builds the element tree, incl. `clean_text` (field & control-char handling) |
| `doc_element_registry.{hpp,cpp}` | Flat element store (id = vector index) + a text side-payload |
| `doc_document.{hpp,cpp}`         | `internal::Document` subclass + the `ElementAdapter` |

`ElementRegistry` is a `vector<Element>` (id = index) with parent/child/sibling
ids and a side map for the text payload; `create_element` / `create_text_element`
/ `append_child` are the only builders.

## Pipeline: how a `.doc` becomes the element tree

1. **Wiring.** `LegacyMicrosoftFile::parse_meta` detects the `/WordDocument`
   stream → `FileType::legacy_word_document`, `DocumentType::text`, and
   `document()` returns `text::Document`.
2. **Read the FIB.** `parse_tree` opens `/WordDocument` and reads the **File
   Information Block** (§2.5.1). The FIB is variable-length and self-describing:
   a fixed `FibBase` (32 B) followed by four counted arrays — `csw`·uint16
   (`fibRgW`), `cslw`·uint32 (`fibRgLw`), `cbRgFcLcb`·`FcLcb` (`fibRgFcLcb`),
   `cswNew`·uint16 (`fibRgCswNew`). `read(ParsedFib&)` reads each count,
   validates it covers the struct we model, then `ignore`s any surplus.
3. **Pick the FIB version.** The effective `nFib` is `fibRgCswNew.nFibNew` when
   `cswNew > 0`, else `FibBase.nFib`. `type_dispatch_FibRgFcLcb` maps it
   (`nFib97 … nFib2007`) to the right `FibRgFcLcb*` layout and `memcpy`s the raw
   `fibRgFcLcb` bytes into it. We only read `clx` out of it, but the whole
   versioned struct is modelled so the offset is correct.
4. **Locate & read the Clx (piece table).** The table stream is `/1Table` or
   `/0Table` per `FibBase.fWhichTblStm`. The Clx (§2.9.38) lives at
   `fibRgFcLcb->clx.fc`. `read_Clx` walks it: leading `Prc` entries (lead
   `0x01`) are skipped, then the `Pcdt` (lead `0x02`) carries the `PlcPcd` — the
   piece table mapping CP ranges to byte offsets in `/WordDocument`.
   `read_character_index` turns it into a `CharacterIndex`.
5. **Concatenate the body text.** Pieces come in ascending CP order;
   `parse_tree` clamps each to the remaining `ccpText` budget (so only the main
   body is taken), seeks to each piece's `data_offset`, decodes it.
6. **Build the tree.** Split the body on `0x0D` (paragraph mark) — dropping the
   trailing empty paragraph from the body's guard mark — then each paragraph on
   `0x0C` (end-of-section / manual page break) and each segment on `0x0B`
   (manual line break):

   ```
   root  (ElementType::root)
   ├── paragraph  (ElementType::paragraph)    split on 0x0D, then 0x0C
   │   ├── text       (ElementType::text)     clean_text(...) of the run
   │   └── line_break (ElementType::line_break)  for 0x0B in a paragraph
   └── page_break (ElementType::page_break)   one per 0x0C boundary
   ```
7. **Render.** HTML works through the generic renderer via the public
   `Paragraph` / `Text` / `LineBreak` API and our adapters.

## The piece table (`CharacterIndex`)

A `.doc` stores text in **pieces** rather than one contiguous run: the `PlcPcd`
is `n+1` ascending CP boundaries (`aCP`) followed by `n` `Pcd` structures
(`aData`). `PlcPcdMap` is a zero-copy view over the raw `plcPcd` bytes computing
`n = (cb - 4) / (4 + sizeof(Pcd))`, exposing `aCP(i)` / `aData(i)`.

Each `Pcd` holds an `FcCompressed`:
- `fCompressed == 0` → **UTF-16**, `data_offset = fc`, 2 bytes per CP.
- `fCompressed == 1` → **compressed** (one byte per CP), `data_offset = fc / 2`.

`read_character_index` records, per piece, `(start_cp, length_cp, data_offset,
is_compressed)`; `CharacterIndex::Iterator` derives `length_cp` from adjacent CP
boundaries and `data_length` from the compression flag. `append` enforces
ascending CP order (throws otherwise).

### Text decoding

- **Uncompressed**: `length_cp` UTF-16 code units → `u16string_to_string`.
- **Compressed**: each byte is one code point (§2.9.73 / §2.4.1 step 6). Bytes
  `0x82–0x9F` are remapped via `uncompress_char` (the Windows-1252 "smart
  quotes" block — e.g. `0x92 → U+2019`, `0x96 → U+2013`); every other byte `b`
  is code point `U+00b` and UTF-8-encoded, so `0xA0–0xFF` round-trip (e.g.
  `0xE9 → "é"`).
- **In-text control characters** (`clean_text`):
  - `0x0D` paragraph mark, `0x0C` end-of-section / manual page break, `0x0B`
    manual line break are consumed by the caller's splits and never reach
    `clean_text`. A `0x0C` boundary emits a `page_break` (§2.8.26).
  - `0x13`/`0x14`/`0x15` delimit a **field**: instruction (begin→separator)
    hidden, result (separator→end) shown. The separator `0x14` is optional
    (§2.8.25); a separator-less field is hidden up to its `0x15` end. Nesting is
    tracked with a per-field stack.
  - `0x09` tab kept; `0x1E` non-breaking hyphen → `-`; `0x1F` optional hyphen
    dropped; all other control characters `< 0x20` (picture/OLE `0x01`, footnote
    ref `0x02`, cell mark `0x07`, …) dropped.

## Adapters

`doc_document.cpp` implements the generic `ElementAdapter` plus
`TextRootAdapter` / `ParagraphAdapter` / `SpanAdapter` / `TextAdapter` /
`LineBreakAdapter`:
- `text_root_page_layout` / `text_root_first_master_page` → empty.
- `paragraph_style` / `span_style` / `line_break_style` → empty (`TODO`).
- `paragraph_text_style` / `text_style` set `font_size = 11pt` so empty
  paragraphs still have height (same hack as the PPT module; removed when
  character formatting lands — see open work).
- `Document::is_editable()` → `true` and `is_savable(encrypted)` →
  `!encrypted`, but `save(...)` and `text_set_content(...)` throw
  `UnsupportedOperation` — read-only in practice.

## Binary format reference (FIB)

The FIB is the root of every `.doc`, at offset 0 of `/WordDocument`:

```
FibBase        32 B fixed   (wIdent, nFib, flags incl. fWhichTblStm/fEncrypted, …)
csw            uint16       count of the following uint16 array
fibRgW         csw·uint16
cslw           uint16       count of the following uint32 array
fibRgLw        cslw·uint32  (holds ccpText at uint16 indices 6–7)
cbRgFcLcb      uint16       count of the following FcLcb (8-byte) array
fibRgFcLcb     cbRgFcLcb·FcLcb   (holds clx → the piece table)
cswNew         uint16       count of the following uint16 array
fibRgCswNew    cswNew·uint16     (nFibNew overrides FibBase.nFib when present)
```

`ccpText` (count of CPs in the main body) is read out of `fibRgLw` as a
little-endian uint32 spanning indices 6–7; it is signed and MUST be ≥ 0, so a
value with the sign bit set **throws** (§2.5.5). `nFib` values handled: `nFib97`
(0x00C1), `nFib2000` (0x00D9), `nFib2002` (0x0101), `nFib2003` (0x010C),
`nFib2007` (0x0112). A value **above** `nFib2007` falls back to the
`FibRgFcLcb2007` layout; a value below `nFib97` **throws**.

---

## Design decisions

**Main body only, via the `ccpText` budget.** `/WordDocument` interleaves the
body with headers, footnotes, annotations, etc.; the FIB's `ccp*` counts
partition the CP space. We take only the first `ccpText` CPs by clamping each
piece to the remaining budget and stopping when exhausted.

**Self-describing FIB read — forward-compatible.** `read(ParsedFib&)` trusts the
on-disk counts rather than a fixed layout: it reads what we model and `ignore`s
the surplus. A FIB from a newer Word that appends fields still parses — the
version dispatch picks the matching `FibRgFcLcb*` (or `FibRgFcLcb2007` for a
newer-than-2007 `nFib`), and the `FcLcb` block is copied **clamped** to
`min(sizeof(layout), cbRgFcLcb·8)`, so extra trailing entries are ignored and a
shorter block leaves the remainder zero (the `clx`/`fcClx` we need lives in the
`FibRgFcLcb97` base, always covered). The `csw`/`cslw` counts must still cover
the arrays we read, else they throw.

**Fail early on malformed input** (matches the sibling `.ppt` parser). We
**throw** on: an `nFib` below `nFib97` or an unknown `nFibNew` (newer-than-2007
`nFib` does **not** throw — it uses the 2007 layout); a `ccpText` with the sign
bit set (§2.5.5); a `csw`/`cslw` count too small to cover the array we read; an
unexpected lead byte while walking the Clx (anything other than `0x01`/`0x02`);
a piece table whose CP boundaries are not ascending; a compressed byte outside
`0x00–0xFF` or an early EOF while decoding. We **pass through** for things we
don't model: text after the main body, the `Prc` formatting runs, and every
control/field character `clean_text` drops.

**Endianness.** Host byte order / LSB-first bit-fields assumed; shared `oldms/`
assumption, analysis and fix plan in [`../AGENTS.md`](../AGENTS.md).

## Tests

- `OldMs.doc_read_string_compressed` — the compressed (1-byte-per-CP) decoder
  against the §2.9.73 byte map: ASCII passthrough, the `0x82–0x9F` remap, the
  `0xA0–0xFF` UTF-8 round-trip.

The FIB-robustness behaviours (negative `ccpText` rejected, newer-than-2007
`nFib` falling back to the 2007 layout) and the `0x0C` page-break emission are
**not yet unit-tested**; there is also **no assertion-based render test** over a
real `.doc` fixture (unlike the `.ppt` cases).

---

# Open work

## 1. Character (font) formatting → the IR (the next feature)

**Goal.** Extract per-run character properties (font name, size, bold, italic,
underline, strikethrough, colour, highlight) and surface them through the
abstract model's `TextStyle`, so the HTML renderer styles text instead of
emitting one flat 11pt run. This replaces the `font_size = 11pt` placeholder in
`doc_document.cpp`.

`TextStyle` (`src/odr/style.hpp`) maps almost 1:1 onto the `.doc` character
SPRMs:

| `TextStyle` field   | SPRM (opcode)            | operand → value                                            |
|---------------------|--------------------------|------------------------------------------------------------|
| `font_size`         | `sprmCHps` (0x4A43)      | u16 **half-points** → `Measure(hps/2.0, pt)` (default 20 = 10pt) |
| `font_weight`       | `sprmCFBold` (0x0835)    | `ToggleOperand` → `FontWeight::bold` when on               |
| `font_style`        | `sprmCFItalic` (0x0836)  | `ToggleOperand` → `FontStyle::italic` when on              |
| `font_underline`    | `sprmCKul` (0x2A3E)      | `Kul` value, `0x00` = none → `bool`                        |
| `font_line_through` | `sprmCFStrike` (0x0837)  | `ToggleOperand` → `bool`                                   |
| `font_color`        | `sprmCCv` (0x6870)       | `COLORREF` → `Color`; legacy `sprmCIco` (0x2A42) is a palette index |
| `background_color`  | `sprmCHighlight` (0x2A0C)| `Ico` highlight index → `Color`                            |
| `font_name`         | `sprmCRgFtc0` (0x4A4F)   | s16 index into `SttbfFfn` → font name (intern it; see below) |

`font_name` is a `const char *`, so the resolved name needs stable storage —
intern it in the `ElementRegistry` (e.g. a `std::deque<std::string>` whose
elements never move) and hand out the pointer.

**How `[MS-DOC]` stores & retrieves character properties** — the authoritative
algorithm is **Direct Character Formatting** (§2.4.6.2), which reuses the
*Retrieving Text* walk we already have:
1. For a character at `cp`, run *Retrieving Text* (§2.4.1) to get its byte
   offset `fc` in `/WordDocument` and the owning `Pcd` (we already compute both).
2. Read the **`PlcBteChpx`** (§2.8.5) at `fcPlcfBteChpx`/`lcbPlcfBteChpx` in the
   table stream — a PLC keyed by **stream offset**: `aFC[n+1]` boundaries +
   `aPnBteChpx[n]` (`PnFkpChpx`, 4 bytes each).
3. Find the largest `i` with `aFC[i] ≤ fc`; read a **`ChpxFkp`** (§2.9.33) at
   `aPnBteChpx[i].pn * 512` in `/WordDocument` (a fixed 512-byte page: `rgfc`
   run boundaries, parallel `rgb` offsets, `crun` in the last byte).
4. Find the largest `j` with `rgfc[j] ≤ fc`; the `Chpx` (§2.9.32) lives at
   `rgb[j] * 2` within the page. `Chpx.grpprl` is an array of **`Prl`** = `Sprm`
   (2 bytes) + operand.
5. Append the `Pcd.Prm` modifications (§2.9.214–216): a `Prm0` (inline) or
   `Prm1` (index) carrying extra SPRMs for this run.

`Prl`/`Sprm` is already modelled in `doc_structs.hpp` (`Sprm` with
`ispmd/fSpec/sgc/spra` and `operand_size()`); a **character** property is a SPRM
with `sgc == 2`. Walk each `Chpx.grpprl` by reading a 2-byte `Sprm` then
`operand_size()` operand bytes (note `spra == 6` is length-prefixed/variable),
keeping only the opcodes above.

**First cut — direct formatting only.** Implement §2.4.6.2 (`Chpx.grpprl` +
`Pcd.Prm`) and map the table's SPRMs. Captures the common case: bold/italic/
size/font/colour applied directly to runs. Resolve `sprmCRgFtc0` by reading
**`SttbfFfn`** (§2.9.286) once at `fcSttbfFfn`/`lcbSttbfFfn` (an STTB of `FFN`
records; `FFN.xszFfn` is the UTF-16 font name) and indexing it. Drop the
hardcoded 11pt; use 10pt (the `sprmCHps` default of 20 half-points).

**Full fidelity — styles (later).** *Determining Formatting Properties*
(§2.4.6.6) layers, in order: document defaults → `STSH` (§2.4.6.5,
`fcStshf`/`lcbStshf`) paragraph- and character-style `grpprl`s resolved via the
paragraph's `istd` → table-style props → direct paragraph → direct character.
The first cut skips the STSH layer, so style-dependent props fall back to
defaults; wiring the STSH closes that gap.

**Wiring to the abstract model.** Today `parse_tree` concatenates all body
pieces into one `body_text` and emits one `text` per paragraph. Per-run styling
needs run boundaries, expressed in `/WordDocument` byte offsets
(`ChpxFkp.rgfc`, `PlcBteChpx.aFC`) — so:
1. **Keep the FC↔text mapping.** While concatenating, retain each piece's
   `data_offset` and compression so any character's source `fc` is recoverable
   (the `CharacterIndex` already holds this; thread it through instead of
   discarding it after building `body_text`).
2. **Split paragraphs into runs.** Within a paragraph, cut at every `ChpxFkp`
   run boundary inside it, resolve each run's `TextStyle` once, and emit a
   **`span`** (`ElementType::span`, already wired via `SpanAdapter`) per run,
   with the `text` element(s) as its children. Paragraph/line-break splitting
   stays as-is.
3. **Store the style.** Add a `TextStyle` side-map to `ElementRegistry` keyed by
   span id (mirror the text side-payload, and the frame-payload pattern in
   `presentation`) plus the font-name intern store. `SpanAdapter::span_style`
   returns the stored style; `text_style` / `paragraph_text_style` then return
   `{}` (or the paragraph mark's run style) instead of the 11pt hack.

## 2. Coverage gaps

- **Only the main document body.** `parse_tree` stops at the `ccpText` budget,
  so headers/footers, footnotes, endnotes, comments/annotations, and text boxes
  — each its own CP range after the body (`ccpFtn`, `ccpHdd`, `ccpAtn`, … in
  FibRgLw97, located via the matching `plcf*` in the table stream) — are
  dropped. Extending coverage means walking the later CP ranges and their
  `Plcf*` structures.
- **Tables.** Cell text renders as plain paragraphs: the end-of-cell mark `0x07`
  is dropped by `clean_text` and row/cell structure (§2.4.3, `sprmPFInTable` /
  `sprmPTtp` / the `TC`/`TAP` tables) is unmodelled. Reconstruct table structure
  from the paragraph properties to emit real `table`/`row`/`cell` elements.
  Paragraph-level formatting (alignment, indent, spacing) via `PlcBtePapx` →
  `PapxFkp` belongs here too, alongside the character work.
- **Fields show only the cached result.** `clean_text` keeps the field *result*
  and drops the *instruction* (§2.8.25); page numbers, dates, refs show their
  last-saved value and are never evaluated. Acceptable for "visible text".
- **Images / OLE / drawn objects.** The anchor characters (`0x01` inline
  picture, `0x08` floating picture, OLE) are dropped. No image extraction; would
  require `PlcfSpa` / the Office Art (`dggInfo`) drawing data.
- **Encrypted / obfuscated documents.** `FibBase.fEncrypted` / `fObfuscated` are
  parsed but not acted on; `decrypt` throws `UnsupportedOperation`.
  XOR-obfuscated and `[MS-OFFCRYPTO]`-encrypted `.doc` are unsupported.

## 3. Smaller shortcomings

- **Endianness.** Shared `oldms/` shortcoming — see [`../AGENTS.md`](../AGENTS.md).
  For `.doc`: every field is read in host byte order, and the
  `FibBase`/`Sprm`/`FcCompressed` bit-fields in `doc_structs.hpp` assume
  LSB-first allocation.

## Reference: the read path

```
WordDocument stream
└─ FIB @ 0                                   [MS-DOC] §2.5.1
   ├─ FibBase (32 B): fWhichTblStm, fEncrypted, nFib
   ├─ csw·u16  fibRgW
   ├─ cslw·u32 fibRgLw  → ccpText (idx 6–7)  §2.5.5
   ├─ cbRgFcLcb·FcLcb fibRgFcLcb → clx.fc    §2.5.7 (version by nFib)
   └─ cswNew·u16 fibRgCswNew → nFibNew

Table stream (/1Table or /0Table per fWhichTblStm)   §1.4
└─ Clx @ clx.fc                               §2.9.38
   ├─ RgPrc: 0..n Prc (lead 0x01, skipped)    §2.9.209
   └─ Pcdt  (lead 0x02)                        §2.9.178
      └─ PlcPcd: aCp[n+1] + aPcd[n] (Pcd)      §2.8.35 / §2.9.177
         └─ Pcd.fc = FcCompressed              §2.9.73
            ├─ fCompressed=0 → UTF-16 @ fc
            └─ fCompressed=1 → 8-bit @ fc/2 (+ 0x82–0x9F map)

Retrieving Text algorithm: §2.4.1 (steps 1–6, matches parse_tree)
Field characters 0x13/0x14/0x15: §2.8.25
```

Character-formatting path (open work §1), keyed by `/WordDocument` byte offset
`fc`:

```
Table stream
├─ PlcBteChpx @ fcPlcfBteChpx                  §2.8.5
│  └─ aFC[n+1] (stream offsets) + aPnBteChpx[n] (PnFkpChpx, 4 B)
├─ SttbfFfn @ fcSttbfFfn  (font names, FFN.xszFfn)   §2.9.286
└─ STSH @ fcStshf  (styles — full fidelity only)     §2.4.6.5

WordDocument stream
└─ ChpxFkp @ aPnBteChpx[i].pn * 512  (512-byte page)  §2.9.33
   ├─ rgfc[crun+1] run boundaries (stream offsets)
   ├─ rgb[crun] → Chpx @ rgb[j]*2 within page
   └─ crun (last byte)
      └─ Chpx = cb + grpprl(Prl[])               §2.9.32
         └─ Prl = Sprm (2 B) + operand            §2.2.x
            └─ character SPRMs have sgc == 2; + Pcd.Prm  §2.9.214–216

Direct Character Formatting: §2.4.6.2  (Determining Formatting Properties: §2.4.6.6)
Font SPRMs: CHps 0x4A43, CFBold 0x0835, CFItalic 0x0836, CKul 0x2A3E,
            CFStrike 0x0837, CCv 0x6870, CHighlight 0x2A0C, CRgFtc0 0x4A4F
```
