# `.doc` (Word) support — design & open work

The **why** and the roadmap; what is concretely done lives in the code. Shared
`oldms/` conventions are in [`../AGENTS.md`](../AGENTS.md).

**Scope.** Extract the **visible text of the main document body**, split into
paragraphs and manual line breaks, plus **direct character formatting** (font,
size, bold, italic, underline, strike, color, highlight) as styled spans,
through the abstract model. No paragraph styles or style-sheet (STSH)
inheritance, headers/footers/notes/annotations, tables, frames, images, or
fields beyond their result text.

**Specs.** `[MS-DOC]` (FIB, Clx / piece table, text decoding, CHPX) +
`[MS-CFB]` container. The implemented read path matches *Retrieving Text*
(§2.4.1 steps 1–6) and *Direct Character Formatting* (§2.4.6.2); section
numbers are cited inline in code and in the read-path maps below.

## Module layout (sibling of `../presentation`)

| File (`oldms/text/`) | Role |
|---|---|
| `doc_structs.hpp` | `#pragma pack(1)` PODs (`FibBase`, the `FibRgFcLcb97/2000/…/2007` chain, `Sprm`, `FcCompressed`, `Pcd`, `PnFkpChpx`, `FfnFixed`) + `static_assert` sizes + `PlcPcdMap`/`PlcBteChpxMap` + `ParsedFib` + the character SPRM opcodes |
| `doc_io.{hpp,cpp}` | `read(...)` over `std::istream`: variable-length FIB, Clx walk, string decoding |
| `doc_helper.{hpp,cpp}` | `CharacterIndex` (decoded piece table) + `read_character_index`; `CharacterRuns` (fc-keyed style-index runs) + `read_character_runs` (PlcBteChpx/ChpxFkp walk) |
| `doc_style.{hpp,cpp}` | `StyleRegistry` (resolved `TextStyle`s by index + font-name intern store), `apply_character_sprms`, `read_font_names` (SttbfFfn) |
| `doc_parser.{hpp,cpp}` | `parse_tree` → styled runs + tree; `TextCleaner` (field & control-char handling) |
| `doc_element_registry.{hpp,cpp}` | Flat element store (id = vector index) + text side-payload + per-element style-index map |
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

**Direct character formatting only, resolved to styled spans.** The tree is
`root → paragraph → span → text`; each span (and each paragraph, for
empty-paragraph height) stores a **style index** in an element-registry
side-map, resolved through the document's `StyleRegistry` (element/style
registry split, mirroring `../spreadsheet`; index 0 is the 10pt default —
the `sprmCHps` default of 20 half-points; the old flat `11pt` placeholder is
gone). The §2.4.6.2 walk: `PlcBteChpx` (table stream) → 512-byte `ChpxFkp`
pages (WordDocument stream) → per-run `Chpx.grpprl`, applied over the default
style. The non-obvious bits:
- **Runs are keyed by stream offset (fc), not CP**, so the piece decode walks
  each piece in style-uniform chunks (`CharacterRuns::chunk_end`); a
  boundary inside a 2-byte CP is pushed past it.
- **Equal `Chpx` bytes share one resolved style**, and adjacent equal-style
  runs merge, so span count stays low. `rgb[j] == 0` means default properties.
- **`ToggleOperand`** (§2.9.327) values `0x80`/`0x81` refer to the (unmodelled)
  style value, which defaults to off → `0x80` = off, `0x81` = on.
- **Colors**: `sprmCCv` is a `COLORREF` (`fAuto` → unset); the legacy
  `sprmCIco`/`sprmCHighlight` use the `Ico` palette (§2.9.119) — the spec's
  extracted table repeats `0x0C` for `0x0D`, which is dark red (`0x800000`).
- **`Pcd.Prm` modifications and STSH styles are not applied** (open work §1).
- Font names from `SttbfFfn` are stored once in the `StyleRegistry` (never
  modified afterwards, like the xls module) so `TextStyle::font_name`
  (`const char *`) stays valid.

`Document::is_editable()` → `false`; `save`/`text_set_content` throw.

**`TextCleaner` control-character handling** (the non-obvious cases): `0x0D`/
`0x0C`/`0x0B` are consumed by the caller's paragraph/page/line splits and never
reach the cleaner; `0x13`/`0x14`/`0x15` delimit a **field** — instruction
hidden, result shown, the `0x14` separator optional (§2.8.25), nesting tracked
with a per-field stack that now **persists across style runs and paragraphs**
(a field can span both); `0x09` tab kept; `0x1E` non-breaking hyphen → `-`;
`0x1F` optional hyphen dropped; every other control char < `0x20` dropped.

## Tests

- `OldMs.doc_read_string_compressed` — the compressed decoder against the §2.9.73
  byte map (ASCII passthrough, `0x82–0x9F` remap, `0xA0–0xFF` round-trip).
- `OldMs.doc_apply_character_sprms` — every modelled SPRM, toggle semantics,
  cvAuto reset, unknown fixed/variable SPRM skipping, malformed-grpprl throws.
- `OldMs.doc_read_font_names` — synthetic SttbfFfn.
- `OldMs.doc_character_formatting` — end-to-end over a synthetic `.doc`
  (inline bytes: FIB + ChpxFkp + piece table): span splitting at run
  boundaries, default 10pt, bold + font-name run, paragraph style.

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

## 1. Character formatting fidelity

Direct formatting (§2.4.6.2) is implemented — see the design notes above and
the read-path map below. Remaining layers of *Determining Formatting
Properties* (§2.4.6.6):

- **`Pcd.Prm` modifications** (§2.9.214–216): per-piece property diffs —
  `Prm0` (one inline SPRM) or `Prm1` (index into the Clx's `Prc` array, which
  `read_character_index` currently skips). Rare for character properties, but
  incremental saves can carry them.
- **STSH style layering** (§2.4.6.5): document defaults → paragraph/character
  style `grpprl`s via the paragraph's `istd` → direct formatting. Without it,
  style-derived properties (e.g. heading fonts defined only in the style
  sheet, the usual Times New Roman default font) fall back to defaults;
  `ToggleOperand` `0x80`/`0x81` resolve against "off".

Character-formatting read path, keyed by `/WordDocument` byte offset `fc`:

```
Table stream
├─ PlcBteChpx @ fcPlcfBteChpx                  §2.8.5   aFC[n+1] + aPnBteChpx[n] (PnFkpChpx, 4 B)
├─ SttbfFfn   @ fcSttbfFfn  (font names, FFN.xszFfn)   §2.9.286
└─ STSH       @ fcStshf     (styles — not read)   §2.4.6.5

WordDocument stream
└─ ChpxFkp @ aPnBteChpx[i].pn * 512  (512-byte page)  §2.9.33
   ├─ rgfc[crun+1] boundaries (stream offsets), rgb[crun] → Chpx @ rgb[j]*2, crun (last byte)
   └─ Chpx = cb + grpprl(Prl[])  §2.9.32   Prl = Sprm(2 B) + operand; char SPRMs sgc==2; + Pcd.Prm §2.9.214–216 (not read)
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
