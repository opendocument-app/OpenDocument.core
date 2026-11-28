# `.doc` (Word) support: design & open work

Shared `oldms/` conventions are in [`../AGENTS.md`](../AGENTS.md).

**Scope.** The visible text of the main document body, split into paragraphs,
manual line breaks and page breaks. Character formatting (font, size, bold,
italic, underline, strike, color, highlight) as styled spans, and paragraph
formatting (alignment, indents, spacing, line height, page break before),
each from the paragraph's style-sheet (STSH) style and the direct formatting
on top. The first section's page size and margins. No character styles, no
headers, footers, notes, annotations, tables, frames, images, columns, and
fields show only their result text.

**Specs.** `[MS-DOC]` (FIB, Clx and piece table, text decoding, CHPX) plus the
`[MS-CFB]` container. The read path follows *Retrieving Text* (§2.4.1 steps
1 to 6), *Direct Paragraph Formatting* (§2.4.6.1), *Direct Character
Formatting* (§2.4.6.2) and *Determining Properties of a Style*
(§2.4.6.5).

## Module layout

| File (`oldms/text/`) | Role |
|---|---|
| `doc_structs.hpp` | `#pragma pack(1)` PODs (`FibBase`, `FibRgFcLcb97`, `Sprm`, `FcCompressed`, `Pcd`, `PnFkpChpx`, `PnFkpPapx`, `Sed`, `FfnFixed`), `PlcMap` (`PlcPcdMap`, `PlcBteChpxMap`, `PlcBtePapxMap`, `PlcfSedMap`), `ParsedFib`, the character, paragraph and section SPRM opcodes |
| `doc_crypto.{hpp,cpp}` | `decrypt`: the WordDocument, Table and Data streams decrypted |
| `doc_io.{hpp,cpp}` | `read(...)` over `std::istream`: variable-length FIB, Clx walk, string decoding, `uncompress_char` |
| `doc_helper.{hpp,cpp}` | `CharacterIndex` (decoded piece table) and `read_character_index`; `FormattingRuns` (fc-keyed runs of property indices), `read_character_runs` and `read_paragraph_runs` over one FKP walker |
| `doc_style.{hpp,cpp}` | `StyleRegistry` (resolved `TextStyle`s and `ParagraphStyle`s by index, the page layout), `StyleSheet` and `read_style_sheet` (STSH), `apply_character_sprms`, `apply_paragraph_sprms`, `apply_section_sprms`, `read_page_layout` (PlcfSed, Sepx), `read_font_names` (SttbfFfn) |
| `doc_parser.{hpp,cpp}` | `parse_tree`: decoded runs, paragraph marks, `StyleResolver` and the tree; `TextCleaner` (fields and control characters) |
| `doc_element_registry.{hpp,cpp}` | The shared `internal::ElementRegistry` plus a text payload, a per-element character style index and a per-paragraph paragraph style index |
| `doc_document.{hpp,cpp}` | `internal::Document` subclass and the `ElementAdapter` |

The tree is `root → paragraph → span → text`, with `line_break` and
`page_break` elements.

## Design decisions

**Main body only, via the `ccpText` budget.** `/WordDocument` interleaves the
body with headers, footnotes and annotations, and the FIB's `ccp*` counts
partition the CP space. The parser takes the first `ccpText` CPs by clamping
each piece to the remaining budget.

**Self-describing FIB read.** Counts bound each FIB array. The parser keeps the
`FibRgFcLcb97` prefix by value; later versions append entries that it does not
use. A shorter block zero-fills missing entries. `cswNew` bounds the version
extension, whose unused fields are skipped. Known versions are validated;
a future base `nFib` may still use the common prefix. No versioned object is
allocated or deleted through a non-virtual base.

**Piece table.** The `PlcPcd` is `n+1` ascending CP boundaries followed by `n`
`Pcd`s. `PlcPcdMap` views the raw bytes and copies each checked entry to avoid
unaligned access. The Clx and font table are bounded by their declared lengths.
Each `Pcd`'s `FcCompressed`: `fCompressed == 0` is UTF-16 at `fc`, `== 1` is
one byte per CP at `fc/2`, with `0x82` to `0x9F` remapped by `uncompress_char`
(§2.9.73) and every other byte `b` mapped to `U+00b`.

**Fail early.** Throw on: `nFib` below `nFib97` or an unknown `nFibNew`; a
`ccpText` with the sign bit set (§2.5.5); a `csw` or `cslw` count too small
for an array the module reads; an unexpected Clx lead byte (not `0x01` or
`0x02`); non-contiguous CP boundaries; piece offsets beyond the 32-bit range;
missing body coverage; an FFN shorter than its fixed part; early EOF. Pass
through what is not modelled: text after the main body, `Prc` formatting
runs, every control or field character `TextCleaner` drops, and a font name
with an odd byte, extra bytes or no NUL.

**Formatting, resolved to styled spans and paragraphs.** Each span and each
paragraph stores a character style index, and each paragraph a paragraph
style index, resolved through the document's `StyleRegistry`. Index 0 is
Normal (istd 0) without direct formatting.

- Style sheet (§2.4.6.5, §2.9.271): each paragraph style (`stk` 1) is its
  UpxPapx and UpxChpx applied on top of its `istdBase`, recursively, and at
  the bottom on the default character style: 10pt (the `sprmCHps` default
  of 20 half-points) in the `Stshif.ftcAsci` font. An istd that names no
  paragraph style is Normal. Character styles (`stk` 2, `sprmCIstd`) are not
  read.
- Paragraphs (§2.4.6.1): the `PlcBtePapx` (table stream), then the 512-byte
  `PapxFkp` pages (WordDocument stream). A paragraph's `GrpPrlAndIstd` is the
  one whose run holds the fc of its paragraph mark, so the decoder records
  that fc for every mark. Its grpprl lies on the istd's paragraph style.
- Characters (§2.4.6.2): the `PlcBteChpx`, then the `ChpxFkp` pages, then each
  run's `Chpx.grpprl` on top of the character properties of the paragraph's
  style.
- Runs are keyed by stream offset (fc), not CP. The piece decode walks each
  piece in chunks of uniform CHPX (`FormattingRuns::chunk_end`). A boundary
  inside a 2-byte CP is pushed past it.
- Equal property bytes share one index; equal (istd, CHPX) and equal PAPX
  share one resolved style. `rgb[j] == 0` and `bOffset == 0` mean none.
- One Prl walker serves every SPRM kind, sizing `sprmTDefTable` (u16 size)
  and `sprmPChgTabs` with size 255 (§2.6.2) apart from the rest.
- `ToggleOperand` (§2.9.327): `0x80` takes the value of the style it applies
  to, `0x81` its opposite.
- Colors: `sprmCCv` is a `COLORREF` (`fAuto` leaves it unset). `sprmCIco` and
  `sprmCHighlight` use the `Ico` palette (§2.9.119). The spec's extracted
  table repeats `0x0C` for `0x0D`, which is dark red (`0x800000`).
- Resolved `TextStyle` values own their font names, copied from `SttbfFfn`.
- Lengths are twips, rendered in inches like DOCX; `sprmPDyaLine` in 240ths
  of a line becomes a percentage. Justification 3 and up (distributed,
  kashida) is justify.

**Page layout.** `text_root_page_layout` is the first section's: the `Sed`
of the `PlcfSed` locates its `Sepx`, whose section SPRMs apply on top of
their defaults (§2.6.4: US Letter, 1.25in sides, 1in top and bottom). A
negative top or bottom margin is taken by its size. A document without a
`Sepx` gets the defaults.

**Tolerated where malformed.** The style sheet, the PAPX and the sections only
refine the layout. Where one throws `std::runtime_error`, its defaults stand
in and the text still opens; `wvware-cand4.doc` has a PapxFkp with a garbage
first boundary. The text, the piece table and the CHPX still fail early.

**`TextCleaner`.** `0x0D`, `0x0C` and `0x0B` never reach it, because the
caller splits paragraphs, pages and lines on them. `0x13`, `0x14` and `0x15`
delimit a field (§2.8.25): the instruction is hidden, the result shown, the
`0x14` separator is optional, and the nesting stack persists across style
runs and paragraphs. `0x09` is kept. `0x1E` (non-breaking hyphen) becomes
`-`. `0x1F` (optional hyphen) and every other control character below `0x20`
are dropped.

`Document::is_editable()` is `false`. `save` and `text_set_content` throw.

**Decryption** ([MS-DOC] 2.2.6). `FibBase.fObfuscated` selects XOR
obfuscation, which is not read. Otherwise the first `FibBase.lKey` bytes of
the Table stream are the RC4 or RC4 CryptoAPI header. The keystream starts at
the beginning of each stream, and the key changes every 512 bytes. The first
68 bytes of WordDocument and the header stay plain; the Data stream is
encrypted from its start. `FibBase` loses `fEncrypted`, `fObfuscated` and
`lKey`, so the parser and `password_encrypted` see a plain document. No
fixture has a Data stream.

## Tests

- `OldMs.doc_read_string_compressed`: the compressed decoder against the
  §2.9.73 byte map.
- `OldMs.doc_apply_character_sprms`: every modelled SPRM, toggle semantics,
  cvAuto reset, unknown SPRM skipping, malformed grpprl throws.
- `OldMs.doc_toggle_operands_follow_the_style`: `0x80` and `0x81` against a
  bold style.
- `OldMs.doc_apply_paragraph_sprms`, `OldMs.doc_apply_section_sprms`: every
  modelled SPRM, both indent generations, line-height kinds, the irregularly
  sized SPRMs, the section defaults.
- `OldMs.doc_read_style_sheet`: an `istdBase` chain, the default font, a
  character style and unused istds falling back to Normal, a cycle.
- `OldMs.doc_paragraph_formatting`: end to end over a synthetic `.doc` (STSH,
  PapxFkp in both size forms, PlcfSed and Sepx).
- `OldMs.doc_read_font_names`: a synthetic SttbfFfn.
- `OldMs.doc_character_formatting`: end to end over a synthetic `.doc` (FIB,
  ChpxFkp, piece table).
- `OldMsEncryption.*`: the `fEncrypted` flag, RC4 and RC4 CryptoAPI
  decryption, the refusal of XOR obfuscation.
- `html_output_test` compares the real `.doc` fixtures against the reference
  output. There is no assertion-based test over a real fixture.

FIB versions, counted extensions, and negative `ccpText` have synthetic coverage.
`page_break` emission still lacks a focused test.

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
├─ PlcBtePapx @ fcPlcfBtePapx                  §2.8.6   aFC[n+1] + aPnBtePapx[n] (PnFkpPapx, 4 B)
├─ PlcfSed    @ fcPlcfSed                      §2.8.26  aCP[n+1] + Sed[n] (12 B, fcSepx → WordDocument)
├─ SttbfFfn   @ fcSttbfFfn  (font names, FFN.xszFfn)   §2.9.286
└─ STSH       @ fcStshf     (LPStshi, then cstd LPStd: Stdf, name, UpxPapx, UpxChpx)   §2.9.271

WordDocument stream
├─ ChpxFkp @ aPnBteChpx[i].pn * 512  (512-byte page)  §2.9.33
│  ├─ rgfc[crun+1] boundaries (stream offsets), rgb[crun] → Chpx @ rgb[j]*2, crun (last byte)
│  └─ Chpx = cb + grpprl(Prl[])  §2.9.32   Prl = Sprm(2 B) + operand; char SPRMs sgc==2; + Pcd.Prm §2.9.214–216 (not read)
├─ PapxFkp @ aPnBtePapx[i].pn * 512  (512-byte page)  §2.9.174
│  ├─ rgfc[cpara+1], rgbx[cpara] (BxPap, 13 B: bOffset), cpara (last byte)
│  └─ PapxInFkp @ bOffset*2: cb, or 0 then cb'  §2.9.175  → GrpPrlAndIstd = istd + grpprl
└─ Sepx @ Sed.fcSepx: i16 cb + grpprl   §2.9.245
```

# Open work

## 1. Formatting fidelity

- `Pcd.Prm` modifications (§2.9.214 to 216): per-piece property diffs, either
  `Prm0` (one inline SPRM) or `Prm1` (an index into the Clx's `Prc` array,
  which `read_character_index` skips). Incremental saves can carry them.
- Character styles: `sprmCIstd` in a CHPX and the `stk` 2 styles it names,
  layered between the paragraph style and the direct formatting.
- Auto spacing (`sprmPFDyaBeforeAuto`, `sprmPFDyaAfterAuto`): the value next
  to the flag is applied as it is.
- Only the first section's page layout. Columns (`sprmSCcolumns`) and later
  sections are not read.

## 2. Coverage gaps

- Only the main body. Headers, footers, footnotes, endnotes, comments and text
  boxes each have their own CP range after the body (`ccpFtn`, `ccpHdd`,
  `ccpAtn` in `FibRgLw97`, via the matching `plcf*`).
- Tables. Cell text renders as plain paragraphs (`0x07` end-of-cell is
  dropped, `TC` and `TAP` are unmodelled). Reconstruct from the paragraph
  properties (`sprmPFInTable`, `sprmPTtp`, via `PlcBtePapx` and `PapxFkp`).
- Images, OLE and drawn objects. Anchor characters are dropped. Needs
  `PlcfSpa` and Office Art (`dggInfo`).
- XOR obfuscation ([MS-DOC] 2.2.6.1, [MS-OFFCRYPTO] 2.3.7.4 to 2.3.7.6)
  throws `MsUnsupportedCryptoAlgorithm`. No fixture has it.
- Endianness. See [`../AGENTS.md`](../AGENTS.md).
