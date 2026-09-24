# `.doc` (Word) support: design & open work

Shared `oldms/` conventions are in [`../AGENTS.md`](../AGENTS.md).

**Scope.** The visible text of the main document body, split into paragraphs,
manual line breaks and page breaks, plus direct character formatting (font,
size, bold, italic, underline, strike, color, highlight) as styled spans. No
paragraph styles, no style-sheet (STSH) inheritance, no headers, footers,
notes, annotations, tables, frames, images, and fields show only their result
text.

**Specs.** `[MS-DOC]` (FIB, Clx and piece table, text decoding, CHPX) plus the
`[MS-CFB]` container. The read path follows *Retrieving Text* (§2.4.1 steps
1 to 6) and *Direct Character Formatting* (§2.4.6.2).

## Module layout

| File (`oldms/text/`) | Role |
|---|---|
| `doc_structs.hpp` | `#pragma pack(1)` PODs (`FibBase`, the `FibRgFcLcb97` to `FibRgFcLcb2007` chain, `Sprm`, `FcCompressed`, `Pcd`, `PnFkpChpx`, `FfnFixed`), `PlcMap` (`PlcPcdMap`, `PlcBteChpxMap`), `ParsedFib`, the character SPRM opcodes |
| `doc_io.{hpp,cpp}` | `read(...)` over `std::istream`: variable-length FIB, Clx walk, string decoding, `uncompress_char` |
| `doc_helper.{hpp,cpp}` | `CharacterIndex` (decoded piece table) and `read_character_index`; `CharacterRuns` (fc-keyed style-index runs) and `read_character_runs` |
| `doc_style.{hpp,cpp}` | `StyleRegistry` (resolved `TextStyle`s by index, font-name store), `apply_character_sprms`, `read_font_names` (SttbfFfn) |
| `doc_parser.{hpp,cpp}` | `parse_tree`: styled runs and the tree; `TextCleaner` (fields and control characters) |
| `doc_element_registry.{hpp,cpp}` | The shared `internal::ElementRegistry` plus a text payload and a per-element style index |
| `doc_document.{hpp,cpp}` | `internal::Document` subclass and the `ElementAdapter` |

The tree is `root → paragraph → span → text`, with `line_break` and
`page_break` elements.

## Design decisions

**Main body only, via the `ccpText` budget.** `/WordDocument` interleaves the
body with headers, footnotes and annotations, and the FIB's `ccp*` counts
partition the CP space. The parser takes the first `ccpText` CPs by clamping
each piece to the remaining budget.

**Self-describing FIB read.** `read(ParsedFib&)` trusts the on-disk counts,
reads what the module models and ignores the surplus. Version dispatch picks
the `FibRgFcLcb*` layout by `nFib` (`nFib97` 0x00C1, `nFib2000` 0x00D9,
`nFib2002` 0x0101, `nFib2003` 0x010C, `nFib2007` 0x0112). A newer `nFib` uses
the 2007 layout. The `FcLcb` block is copied clamped to
`min(sizeof(layout), cbRgFcLcb·8)`, so extra entries are ignored and a shorter
block leaves the rest zero. `clx` sits in the `FibRgFcLcb97` base, so it is
always covered.

**Piece table.** The `PlcPcd` is `n+1` ascending CP boundaries followed by `n`
`Pcd`s. `PlcPcdMap` is a zero-copy view over the raw bytes. Each `Pcd`'s
`FcCompressed`: `fCompressed == 0` is UTF-16 at `fc`, `== 1` is one byte per
CP at `fc/2`, with `0x82` to `0x9F` remapped by `uncompress_char` (§2.9.73)
and every other byte `b` mapped to `U+00b`.

**Fail early.** Throw on: `nFib` below `nFib97` or an unknown `nFibNew`; a
`ccpText` with the sign bit set (§2.5.5); a `csw` or `cslw` count too small
for an array the module reads; an unexpected Clx lead byte (not `0x01` or
`0x02`); non-ascending CP boundaries; a bad compressed byte; early EOF. Pass
through what is not modelled: text after the main body, `Prc` formatting
runs, and every control or field character `TextCleaner` drops.

**Direct character formatting, resolved to styled spans.** Each span and each
paragraph stores a style index in the element registry, resolved through the
document's `StyleRegistry`. Index 0 is the default style, 10pt (the
`sprmCHps` default of 20 half-points). The §2.4.6.2 walk: `PlcBteChpx` (table
stream), then the 512-byte `ChpxFkp` pages (WordDocument stream), then each
run's `Chpx.grpprl` applied over the default style.

- Runs are keyed by stream offset (fc), not CP. The piece decode walks each
  piece in style-uniform chunks (`CharacterRuns::chunk_end`). A boundary
  inside a 2-byte CP is pushed past it.
- Equal `Chpx` bytes share one resolved style, and adjacent equal-style runs
  merge. `rgb[j] == 0` means default properties.
- `ToggleOperand` (§2.9.327): `0x80` and `0x81` refer to the unmodelled style
  value, which defaults to off, so `0x80` is off and `0x81` is on.
- Colors: `sprmCCv` is a `COLORREF` (`fAuto` leaves it unset). `sprmCIco` and
  `sprmCHighlight` use the `Ico` palette (§2.9.119). The spec's extracted
  table repeats `0x0C` for `0x0D`, which is dark red (`0x800000`).
- Font names from `SttbfFfn` live in the `StyleRegistry` and never move, so
  `TextStyle::font_name` (`const char *`) stays valid.

**`TextCleaner`.** `0x0D`, `0x0C` and `0x0B` never reach it, because the
caller splits paragraphs, pages and lines on them. `0x13`, `0x14` and `0x15`
delimit a field (§2.8.25): the instruction is hidden, the result shown, the
`0x14` separator is optional, and the nesting stack persists across style
runs and paragraphs. `0x09` is kept. `0x1E` (non-breaking hyphen) becomes
`-`. `0x1F` (optional hyphen) and every other control character below `0x20`
are dropped.

`Document::is_editable()` is `false`. `save` and `text_set_content` throw.
`text_root_page_layout` returns `{}`.

## Tests

- `OldMs.doc_read_string_compressed`: the compressed decoder against the
  §2.9.73 byte map.
- `OldMs.doc_apply_character_sprms`: every modelled SPRM, toggle semantics,
  cvAuto reset, unknown SPRM skipping, malformed grpprl throws.
- `OldMs.doc_read_font_names`: a synthetic SttbfFfn.
- `OldMs.doc_character_formatting`: end to end over a synthetic `.doc` (FIB,
  ChpxFkp, piece table).
- `OldMsEncryption.*`: the `fEncrypted` flag.
- `html_output_test` compares the real `.doc` fixtures against the reference
  output. There is no assertion-based test over a real fixture.

Not tested: FIB robustness (negative `ccpText`, newer-than-2007 fallback) and
`page_break` emission.

## Binary format reference (FIB)

At offset 0 of `/WordDocument`. Each counted array follows its count:

```
FibBase        32 B fixed   (wIdent, nFib, flags incl. fWhichTblStm/fEncrypted, …)
csw            u16          → fibRgW       csw·u16
cslw           u16          → fibRgLw      cslw·u32  (ccpText at u16 idx 6–7, §2.5.5)
cbRgFcLcb      u16          → fibRgFcLcb   cbRgFcLcb·FcLcb  (clx → piece table; version by nFib)
cswNew         u16          → fibRgCswNew  cswNew·u16  (nFibNew overrides nFib when present)
```

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

Table stream
├─ PlcBteChpx @ fcPlcfBteChpx                  §2.8.5   aFC[n+1] + aPnBteChpx[n] (PnFkpChpx, 4 B)
├─ SttbfFfn   @ fcSttbfFfn  (font names, FFN.xszFfn)   §2.9.286
└─ STSH       @ fcStshf     (styles, not read)   §2.4.6.5

WordDocument stream
└─ ChpxFkp @ aPnBteChpx[i].pn * 512  (512-byte page)  §2.9.33
   ├─ rgfc[crun+1] boundaries (stream offsets), rgb[crun] → Chpx @ rgb[j]*2, crun (last byte)
   └─ Chpx = cb + grpprl(Prl[])  §2.9.32   Prl = Sprm(2 B) + operand; char SPRMs sgc==2; + Pcd.Prm §2.9.214–216 (not read)
```

# Open work

## 1. Character formatting fidelity

The remaining layers of *Determining Formatting Properties* (§2.4.6.6):

- `Pcd.Prm` modifications (§2.9.214 to 216): per-piece property diffs, either
  `Prm0` (one inline SPRM) or `Prm1` (an index into the Clx's `Prc` array,
  which `read_character_index` skips). Incremental saves can carry them.
- STSH style layering (§2.4.6.5): document defaults, then the paragraph and
  character style `grpprl`s via the paragraph's `istd`, then direct
  formatting. Without it, style-derived properties fall back to the defaults
  and `ToggleOperand` resolves against "off".

## 2. Coverage gaps

- Only the main body. Headers, footers, footnotes, endnotes, comments and text
  boxes each have their own CP range after the body (`ccpFtn`, `ccpHdd`,
  `ccpAtn` in `FibRgLw97`, via the matching `plcf*`).
- Tables. Cell text renders as plain paragraphs (`0x07` end-of-cell is
  dropped, `TC` and `TAP` are unmodelled). Reconstruct from the paragraph
  properties (`sprmPFInTable`, `sprmPTtp`, via `PlcBtePapx` and `PapxFkp`).
- No page layout. `text_root_page_layout` returns `{}`, so the page box hugs
  the text. Section properties (`PlcfSed`, `Sepx`, `sprmSXaPage`,
  `sprmSYaPage`, `sprmSDxaLeft`, `sprmSDxaRight`, `sprmSDyaTop`,
  `sprmSDyaBottom`, §2.6.4) are unparsed.
- Images, OLE and drawn objects. Anchor characters are dropped. Needs
  `PlcfSpa` and Office Art (`dggInfo`).
- Decryption ([MS-OFFCRYPTO]). See [`../AGENTS.md`](../AGENTS.md).
- Endianness. See [`../AGENTS.md`](../AGENTS.md).
