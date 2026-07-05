# `.doc` (Word) support — design & open work

The **why** and the roadmap; what is concretely done lives in the code. Shared
`oldms/` conventions are in [`../AGENTS.md`](../AGENTS.md).

**Scope.** Extract the **visible text of the main document body**, split into
paragraphs and manual line breaks, through the abstract model so the generic HTML
renderer lays it out as a flat run of paragraphs. No character/paragraph styles,
headers/footers/notes/annotations, tables, frames, images, or fields beyond their
result text.

**Specs.** `[MS-DOC]` (FIB, Clx / piece table, text decoding) + `[MS-CFB]`
container. The implemented read path matches *Retrieving Text* (§2.4.1 steps 1–6);
section numbers are cited inline in code and in the read-path map below.

## Module layout (sibling of `../presentation`)

| File (`oldms/text/`) | Role |
|---|---|
| `doc_structs.hpp` | `#pragma pack(1)` PODs (`FibBase`, the `FibRgFcLcb97/2000/…/2007` chain, `Sprm`, `FcCompressed`, `Pcd`) + `static_assert` sizes + `PlcPcdMap` + `ParsedFib` |
| `doc_io.{hpp,cpp}` | `read(...)` over `std::istream`: variable-length FIB, Clx walk, string decoding |
| `doc_helper.{hpp,cpp}` | `CharacterIndex` (decoded piece table) + `read_character_index` |
| `doc_parser.{hpp,cpp}` | `parse_tree` → body text + tree; `clean_text` (field & control-char handling) |
| `doc_element_registry.{hpp,cpp}` | Flat element store (id = vector index) + text side-payload |
| `doc_document.{hpp,cpp}` | `internal::Document` subclass + the `ElementAdapter` |

## Design decisions

**Main body only, via the `ccpText` budget.** `/WordDocument` interleaves the body
with headers, footnotes, annotations, etc.; the FIB's `ccp*` counts partition the
CP space. We take only the first `ccpText` CPs by clamping each piece to the
remaining budget and stopping when exhausted.

**Self-describing FIB read — forward-compatible.** `read(ParsedFib&)` trusts the
on-disk counts, not a fixed layout: it reads what we model and `ignore`s the
surplus. A FIB from a newer Word still parses — version dispatch picks the matching
`FibRgFcLcb*` (or `FibRgFcLcb2007` for a newer-than-2007 `nFib`), and the `FcLcb`
block is copied **clamped** to `min(sizeof(layout), cbRgFcLcb·8)`, so extra
trailing entries are ignored and a shorter block leaves the remainder zero (the
`clx`/`fcClx` we need is in the `FibRgFcLcb97` base, always covered). The
`csw`/`cslw` counts must still cover the arrays we read, else they throw.

**Piece table, not one contiguous run.** A `.doc` stores text in **pieces**; the
`PlcPcd` is `n+1` ascending CP boundaries followed by `n` `Pcd`s. `PlcPcdMap` is a
zero-copy view over the raw bytes (`n = (cb − 4)/(4 + sizeof(Pcd))`). Each `Pcd`'s
`FcCompressed`: `fCompressed == 0` → UTF-16 at `fc` (2 B/CP); `== 1` → compressed
at `fc/2` (1 B/CP, `0x82–0x9F` remapped via `uncompress_char` — the Windows-1252
smart-quotes block; every other byte `b` → `U+00b`, so `0xA0–0xFF` round-trip).
`append` enforces ascending CP order (throws otherwise).

**Fail early on malformed input** (matches the sibling `.ppt` parser). **Throw**
on: `nFib` below `nFib97` or an unknown `nFibNew` (newer-than-2007 does *not*
throw — uses the 2007 layout); `ccpText` with the sign bit set (§2.5.5, it's
signed and MUST be ≥ 0); a `csw`/`cslw` count too small to cover an array we read;
an unexpected Clx lead byte (not `0x01`/`0x02`); non-ascending CP boundaries; a bad
compressed byte or early EOF. **Pass through** what we don't model: text after the
main body, `Prc` formatting runs, and every control/field char `clean_text` drops.

**Endianness.** Host byte order / LSB-first bit-fields assumed; shared `oldms/`
assumption + fix plan in [`../AGENTS.md`](../AGENTS.md).

**The `font_size = 11pt` in the adapters is a placeholder hack** so empty
paragraphs still have height (same as the `.ppt`/`.xls` modules); removed when
character formatting lands (open work §1). `Document::is_editable()` → `true` and
`is_savable(!encrypted)`, but `save`/`text_set_content` throw — read-only in
practice.

**`clean_text` control-character handling** (the non-obvious cases): `0x0D`/`0x0C`/
`0x0B` are consumed by the caller's paragraph/page/line splits and never reach
`clean_text`; `0x13`/`0x14`/`0x15` delimit a **field** — instruction hidden,
result shown, the `0x14` separator optional (§2.8.25), nesting tracked with a
per-field stack; `0x09` tab kept; `0x1E` non-breaking hyphen → `-`; `0x1F`
optional hyphen dropped; every other control char < `0x20` dropped.

## Tests

- `OldMs.doc_read_string_compressed` — the compressed decoder against the §2.9.73
  byte map (ASCII passthrough, `0x82–0x9F` remap, `0xA0–0xFF` round-trip).

**Not yet tested:** FIB robustness (negative `ccpText`, newer-than-2007 fallback),
`0x0C` page-break emission, and there is **no assertion-based render test** over a
real `.doc` fixture (unlike `.ppt`).

## Binary format reference (FIB)

Root of every `.doc`, at offset 0 of `/WordDocument`. Variable-length,
self-describing — each counted array follows its count:

```
FibBase        32 B fixed   (wIdent, nFib, flags incl. fWhichTblStm/fEncrypted, …)
csw            u16          → fibRgW       csw·u16
cslw           u16          → fibRgLw      cslw·u32  (ccpText at u16 idx 6–7, §2.5.5)
cbRgFcLcb      u16          → fibRgFcLcb   cbRgFcLcb·FcLcb  (clx → piece table; version by nFib)
cswNew         u16          → fibRgCswNew  cswNew·u16  (nFibNew overrides nFib when present)
```

`nFib` handled: `nFib97` 0x00C1, `nFib2000` 0x00D9, `nFib2002` 0x0101, `nFib2003`
0x010C, `nFib2007` 0x0112. Above 2007 → `FibRgFcLcb2007` layout; below 97 throws.

### Read path

```
WordDocument stream
└─ FIB @ 0                                   [MS-DOC] §2.5.1
   ├─ FibBase (32 B): fWhichTblStm, fEncrypted, nFib
   ├─ cslw·u32 fibRgLw  → ccpText (idx 6–7)  §2.5.5
   └─ cbRgFcLcb·FcLcb fibRgFcLcb → clx.fc    §2.5.7 (version by nFib)

Table stream (/1Table or /0Table per fWhichTblStm)   §1.4
└─ Clx @ clx.fc                               §2.9.38
   ├─ RgPrc: 0..n Prc (lead 0x01, skipped)    §2.9.209
   └─ Pcdt  (lead 0x02)                        §2.9.178
      └─ PlcPcd: aCp[n+1] + aPcd[n] (Pcd)      §2.8.35 / §2.9.177
         └─ Pcd.fc = FcCompressed              §2.9.73
            ├─ fCompressed=0 → UTF-16 @ fc
            └─ fCompressed=1 → 8-bit @ fc/2 (+ 0x82–0x9F map)

Retrieving Text: §2.4.1 (steps 1–6)   Field chars 0x13/0x14/0x15: §2.8.25
```

---

# Open work

## 1. Character (font) formatting → the IR (the next feature)

**Goal.** Extract per-run character properties (font name, size, bold, italic,
underline, strike, colour, highlight) and surface them through `TextStyle`, so the
renderer styles text instead of emitting one flat 11pt run. Replaces the
`font_size = 11pt` placeholder.

`TextStyle` (`src/odr/style.hpp`) maps almost 1:1 onto the `.doc` character SPRMs:

| `TextStyle` field | SPRM (opcode) | operand → value |
|---|---|---|
| `font_size` | `sprmCHps` (0x4A43) | u16 **half-points** → `Measure(hps/2, pt)` (default 20 = 10pt) |
| `font_weight` | `sprmCFBold` (0x0835) | `ToggleOperand` → `bold` |
| `font_style` | `sprmCFItalic` (0x0836) | `ToggleOperand` → `italic` |
| `font_underline` | `sprmCKul` (0x2A3E) | `Kul`, `0x00` = none → `bool` |
| `font_line_through` | `sprmCFStrike` (0x0837) | `ToggleOperand` → `bool` |
| `font_color` | `sprmCCv` (0x6870) | `COLORREF` → `Color` (legacy `sprmCIco` 0x2A42 is a palette index) |
| `background_color` | `sprmCHighlight` (0x2A0C) | `Ico` highlight index → `Color` |
| `font_name` | `sprmCRgFtc0` (0x4A4F) | s16 index into `SttbfFfn` → font name |

`font_name` is a `const char *`, so the resolved name needs stable storage — intern
it in the `ElementRegistry` (e.g. a `std::deque<std::string>` whose elements never
move) and hand out the pointer.

**How `[MS-DOC]` retrieves character properties** — Direct Character Formatting
(§2.4.6.2) reuses the *Retrieving Text* walk we already have:
1. For `cp`, run §2.4.1 to get its byte offset `fc` and owning `Pcd` (already
   computed).
2. Read **`PlcBteChpx`** (§2.8.5) at `fcPlcfBteChpx` in the table stream — a PLC
   keyed by stream offset: `aFC[n+1]` + `aPnBteChpx[n]` (`PnFkpChpx`, 4 B).
3. Largest `i` with `aFC[i] ≤ fc` → read a **`ChpxFkp`** (§2.9.33) at
   `aPnBteChpx[i].pn * 512` (fixed 512-byte page: `rgfc` boundaries, parallel
   `rgb`, `crun` in the last byte).
4. Largest `j` with `rgfc[j] ≤ fc` → the `Chpx` (§2.9.32) at `rgb[j] * 2` within
   the page. `Chpx.grpprl` is an array of `Prl` = `Sprm` (2 B) + operand.
5. Append `Pcd.Prm` mods (§2.9.214–216): `Prm0` (inline) or `Prm1` (index).

`Prl`/`Sprm` is already modelled (`Sprm` with `ispmd/fSpec/sgc/spra` +
`operand_size()`); a **character** property is a SPRM with `sgc == 2` (note
`spra == 6` is length-prefixed/variable).

**First cut — direct formatting only.** Implement §2.4.6.2 (`Chpx.grpprl` +
`Pcd.Prm`) and map the SPRMs above (bold/italic/size/font/colour applied directly
to runs). Resolve `sprmCRgFtc0` by reading **`SttbfFfn`** (§2.9.286) once and
indexing it. Drop the 11pt; use 10pt (the `sprmCHps` default).

**Full fidelity — styles (later).** *Determining Formatting Properties* (§2.4.6.6)
layers: document defaults → `STSH` (§2.4.6.5) para/char-style `grpprl`s via the
paragraph's `istd` → table-style → direct paragraph → direct character. The first
cut skips STSH (style-dependent props fall back to defaults).

**Wiring to the abstract model.** Per-run styling needs run boundaries in
`/WordDocument` byte offsets, so:
1. **Keep the FC↔text mapping** while concatenating (thread the `CharacterIndex`
   through instead of discarding it after `body_text`).
2. **Split paragraphs into runs** at every `ChpxFkp` boundary, resolve each run's
   `TextStyle`, emit a **`span`** (already wired via `SpanAdapter`) per run.
3. **Store the style** in a `TextStyle` side-map keyed by span id (mirror the text
   side-payload + the `presentation` frame-payload) plus the font-name intern
   store. `SpanAdapter::span_style` returns it; `text_style`/
   `paragraph_text_style` then return `{}` instead of the 11pt hack.

Character-formatting read path, keyed by `/WordDocument` byte offset `fc`:

```
Table stream
├─ PlcBteChpx @ fcPlcfBteChpx                  §2.8.5   aFC[n+1] + aPnBteChpx[n] (PnFkpChpx, 4 B)
├─ SttbfFfn   @ fcSttbfFfn  (font names, FFN.xszFfn)   §2.9.286
└─ STSH       @ fcStshf     (styles — full fidelity only)   §2.4.6.5

WordDocument stream
└─ ChpxFkp @ aPnBteChpx[i].pn * 512  (512-byte page)  §2.9.33
   ├─ rgfc[crun+1] boundaries (stream offsets), rgb[crun] → Chpx @ rgb[j]*2, crun (last byte)
   └─ Chpx = cb + grpprl(Prl[])  §2.9.32   Prl = Sprm(2 B) + operand; char SPRMs sgc==2; + Pcd.Prm §2.9.214–216
```

## 2. Coverage gaps

- **Only the main body.** Headers/footers, footnotes, endnotes, comments, text
  boxes — each its own CP range after the body (`ccpFtn`/`ccpHdd`/`ccpAtn`/… in
  FibRgLw97, via the matching `plcf*`) — are dropped.
- **Tables.** Cell text renders as plain paragraphs (`0x07` end-of-cell dropped;
  `TC`/`TAP` unmodelled). Reconstruct from paragraph properties (`sprmPFInTable`/
  `sprmPTtp`) to emit real `table`/`row`/`cell`. Paragraph-level formatting
  (`PlcBtePapx` → `PapxFkp`) belongs here too.
- **Fields show only the cached result** (§2.8.25) — never evaluated. Acceptable
  for "visible text".
- **Images / OLE / drawn objects** — anchor chars dropped; would need `PlcfSpa` /
  Office Art (`dggInfo`).
- **Encrypted / obfuscated** — `fEncrypted`/`fObfuscated` parsed but not acted on;
  `decrypt` throws.

## 3. Smaller shortcomings

- **Endianness** — shared `oldms/` shortcoming, see [`../AGENTS.md`](../AGENTS.md).
  For `.doc`: fields read in host byte order; `FibBase`/`Sprm`/`FcCompressed`
  bit-fields assume LSB-first.
