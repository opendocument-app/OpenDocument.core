# In-house PDF support — plan (high level)

Companion to [`STATUS.md`](STATUS.md). This is deliberately **high level**:
each stage gets its own detailed design (file layout, structs, test fixtures)
before implementation starts. Stages are ordered by what they unlock; 0–2 are
roughly sequential, 3 and 4 are independent of each other, 5 builds on
whatever pages already render.

**Goal.** Faithful read-only HTML for common real-world PDFs through the odr
engine, so the poppler/pdf2htmlEX engine becomes optional rather than
required.

---

## Stage 0 — file-format compatibility (prerequisite)

Most modern producers write PDF 1.5+ structures that the current parser
rejects outright; nothing else matters until these parse.
**Detailed design: [`PLAN-stage0.md`](PLAN-stage0.md).**

- **Cross-reference streams** (`/Type /XRef`) and **object streams**
  (`/Type /ObjStm`) — the biggest blocker; today `parse_document` throws
  `expected xref` on such files. Includes **hybrid-reference files**
  (`XRefStm` in the trailer, spec 7.5.8.4).
- **Filter framework**: consult `/Filter` (today FlateDecode is assumed
  unconditionally), support Flate **with PNG predictors** (ubiquitous in
  xref streams), ASCIIHex/ASCII85, LZW; pass `DCTDecode`/`JPXDecode`
  payloads through untouched (they become `<img>` data in stage 4).
- **Inherited page attributes**: resolve `MediaBox`, `Resources`, `CropBox`,
  `Rotate` (the complete inheritable set per spec Table 30) through the
  `Pages` ancestor chain.
- **Encryption** (standard security handler): RC4 and AES-128 per
  ISO 32000-1; AES-256 (`R 6`/`AESV3`) per ISO 32000-2 (offline copy under
  `offline/documentation/PDF/ISO_32000-2/`) — real files need it (our
  `encrypted_fontfile3_opentype.pdf` fixture is R 6). Can trail the rest of
  stage 0; wire `password_encrypted()` / `decrypt()` properly.
- Later: xref recovery by scanning for `n g obj` when the table is broken.

## Stage 1 — text extraction: the code → Unicode chain

PDF strings are **character codes**; per font, walk this chain and record
per-code Unicode (or "unknown", which stage 3 handles):

1. **`ToUnicode` CMap** — extend the existing `CMap`: `bfrange`,
   `codespacerange` (multi-byte codes), multi-character targets.
2. **Simple fonts**: `/Encoding` base (WinAnsi/MacRoman/Standard) +
   `/Differences` → glyph names → Unicode via the Adobe Glyph List
   (incl. `uniXXXX`/`uXXXXXX` names).
3. **Composite (Type0/CID) fonts**: `Identity-H/V` plus the predefined CMaps
   (CJK); map CID → Unicode via the CID system info where defined.
4. **Embedded font fallback** (needs stage 3's font *reading*): reverse the
   TrueType `cmap` table; read glyph names from Type1/CFF charstrings.
5. Nothing applies → mark the run "no Unicode" for stage 3's re-encoding
   workaround.

Where the content stream carries `/ActualText` (tagged PDFs, ligatures), it
overrides the whole chain for extraction.

## Stage 2 — text positioning & metrics

Independent of Unicode work; fixes layout even with today's partial CMaps.

- Apply the full transform: text matrix × CTM (both are already tracked in
  `GraphicsState` but never applied), text rise, horizontal scaling.
- **Glyph advances**: `/Widths` + `/MissingWidth` (simple fonts), `/W` +
  `/DW` (CID fonts), char/word spacing, the numeric adjustments in `TJ` —
  so `TJ`, `'`, `"` finally render and `Tj` runs land where they should.
- **Form XObjects** (`Do` on a `/Form`): recursive content-stream execution
  with scoped `/Resources` and the form matrix. Many producers put most page
  content inside forms, and tiling patterns (stage 4) and annotation
  appearances (stage 5) run on the same machinery — a structural
  prerequisite, not a nicety.
- **Text render modes** (`Tr`): mode 3 — invisible text, the OCR-over-scan
  pattern — must stay selectable but unpainted (the inverse of stage 3's PUA
  case); stroke/clip modes (1–2, 4–7) just need graceful degradation.
- **Space inference**: PDFs routinely encode no space characters; insert
  them from glyph-gap heuristics (as pdf2htmlEX does) so copy/paste and
  search work.
- Layout side of bidi (RTL run ordering) and vertical writing
  (Identity-V/CJK) — the CMap side is stage 1.
- Decide the HTML mapping: per-run spans with CSS `transform` (cheap, breaks
  on heavy kerning) vs. per-glyph positioning (exact, verbose) — likely
  per-run with a kerning threshold that splits runs, like pdf2htmlEX.

## Stage 3 — fonts in HTML

Needed for visual fidelity regardless of text extraction.

**Decision (2026-06): in-house, no FontForge.** pdf.js proves complete font
conversion is doable without any font library; pdf2htmlEX uses FontForge for
repair-and-write-anything at the cost of a notoriously heavy build — the
dependency profile this project avoids. No trimmed off-the-shelf alternative
does what we need (FreeType/stb_truetype are read-only; hb-subset can write
but only subsets along the *existing* `cmap`, so it cannot inject the PUA
mappings below). Expected size: ~5–8k lines of focused C++ — on the order of
an `oldms/` module. Reading (SFNT tables, CFF charsets) is the easy part and
is needed by stage 1.4 anyway.

**Architecture: IR for facts, pass-through for glyphs.** No glyph-level font
IR (the document-IR analogue): decompiling and recompiling outlines is the
FontForge model — it loses hinting and risks fidelity, and with one output
format (SFNT) the M×N payoff of an IR never materializes. Glyph data
(outlines, hinting, charstrings) passes through byte-for-byte; even
Type1 → Type2 charstrings is a direct sibling-format translation (hints
preserved), not a trip through a generic outline model. What *is* shared: a
thin `FontProgram`-style interface — per-flavor readers producing the facts
every consumer needs (glyph count, glyph → Unicode, advance widths,
units-per-em, name, bbox, symbolic flag) with the raw bytes kept alongside.
Stage 1.4 reads Unicode from it, the OTF wrap synthesizes
`head`/`hhea`/`hmtx`/`OS/2` from it, the re-encoder assigns PUA code points
from its glyph count.

**Intermediate milestone — fonts as first-class library citizens.** Before
wiring fonts into the PDF HTML output, ship them standalone:

- **Font files as a `DecodedFile` type** (precedent: `SvmFile`, `ImageFile`):
  `FileType` entries + magic detection (SFNT `0x00010000`/`OTTO`/`wOFF`), a
  `FontFile` category, and `html::translate(FontFile)` emitting a **specimen
  page** — name/metrics header plus a glyph grid, with the font served as an
  HTML resource via `@font-face`. Keep the UI at "specimen page"; the cost is
  public API surface, so no font-editor scope creep.
- The glyph grid must show **every** glyph, including ones no `cmap` entry
  reaches — which forces building the PUA re-encoding, table-directory
  rebuild, and OTF wrap *first*, against a directly viewable deliverable with
  font-only tests (no PDF parsing in the loop).
- **In parallel: PDF as a container.** Expose embedded fonts as an
  `abstract::Filesystem` (`/fonts/F1.ttf`, …) and reuse the existing
  filesystem HTML service (as for ZIP/CFB archives). The page-tree walk
  already enumerates fonts; extraction needs only stage 0's `/Filter`
  handling. Doubles as the corpus harvester: weird real-world embedded fonts
  go straight into test fixtures.

Sub-stages, ordered by corpus frequency; each ships independently useful
output:

1. **TrueType** (`FontFile2`, CIDFontType2 — the bulk of modern PDFs): serve
   nearly as-is via `@font-face`; implement the `cmap` rewrite (build a
   format-4/12 subtable, splice the table directory, recompute
   `head.checkSumAdjustment`).
2. **Bare CFF** (`FontFile3`/Type1C — the other big slice): wrap into an OTF
   container by synthesizing the ~8 required tables (`head`, `hhea`, `hmtx`,
   `maxp`, `cmap`, `name`, `OS/2`, `post`); take advance widths from the
   PDF's `/Widths`/`/W` rather than interpreting charstrings.
3. **Type1** (`FontFile` — older docs, and pdfTeX output, so academic PDFs):
   `eexec` decryption, Type1 charstrings → Type2, build a CFF, then reuse
   sub-stage 2. The hardest single piece but precisely specified (Adobe T1
   spec; pdf.js as reference implementation).
4. **Type3** (drawing procedures, no font file — rare but common in
   scientific plots, e.g. older matplotlib) → SVG glyphs, reusing stage 4's
   path rendering; plus **non-embedded fonts**: substitute the standard 14
   and common names with CSS fallbacks + metrics from `/Widths`.

Mechanisms and guards:

- **Re-encoding for unmapped glyphs** (the general workaround): rewrite the
  font's `cmap` so deterministic Private-Use-Area code points
  (`U+E000 + glyph index`) map to the glyphs, emit those code points in the
  HTML, and mark such runs non-extractable in UX (`user-select: none`,
  `aria-hidden`). Display is correct; copy/search is knowingly garbage.
  Option to consider: re-encode *all* fonts this way (pdf2htmlEX's choice)
  for one uniform pipeline.
- **Broken-font long tail**: real embedded fonts are routinely malformed, and
  browsers run web fonts through a sanitizer (OTS) that silently rejects
  them. Regenerating the table directory (which the re-encode/wrap does
  anyway) covers most of it; start strict, add repair heuristics only as real
  files demand. Gate in CI: run **OTS** over every produced font (test-time
  dependency only); optionally FreeType as a second test oracle. Neither
  ships in the product.

## Stage 4 — graphics

**Decision (2026-06): SVG generation, no rasterizer.** pdf2htmlEX uses
poppler to render everything non-text into a per-page background image; we
generate SVG instead — that is serialization, not rendering, and pdf.js
proves the full PDF graphics model needs no native renderer (its canvas
backend, and formerly an SVG one). The PDF and SVG imaging models are close
cousins (PostScript heritage), so the mapping is mostly mechanical. The
trade-off: pdf2htmlEX gets the long tail of weird content right for free via
poppler, while our fidelity is bounded by operator coverage — countered by
the test oracle below. The earlier rasterized-background fallback idea is
**rejected**: it reintroduces exactly the renderer dependency this stage
exists to avoid.

- Vector content → inline SVG per page, layered under the text spans:
  paths, fill rules (nonzero/evenodd), stroke parameters (width, caps,
  joins, miter, dash), transforms; clipping → nested `<clipPath>`; tiling
  patterns → `<pattern>` (form-XObject machinery from stage 2);
  axial/radial shadings (types 2/3) → `linearGradient`/`radialGradient`.
- **Images**: `DCTDecode` → `<img>` JPEG pass-through; Flate/LZW raster →
  PNG encode; inline images (`BI`/`ID`/`EI` — currently not even tokenized
  correctly past `ID`); image masks and SMasks later.
- **SVG residue** — where no 1:1 primitive exists; all handled at
  generation time with computation, never rasterization:
  - mesh/function shadings (types 1, 4–7) → tessellate into small
    flat-colored polygons (pdf.js's approach); PDF radial shadings are
    also slightly more general than SVG's (independent radii, extend
    flags) — approximate the edge cases;
  - color spaces (Separation/DeviceN/Indexed/Lab/ICC) → convert to RGB
    when emitting: sample the tint-transform functions, approximate ICC
    as sRGB at first; ignore overprint like everyone else;
  - transparency: constant alpha (`CA`/`ca`) → `opacity`, soft masks →
    SVG `<mask>`, blend modes → `mix-blend-mode`; isolated/knockout
    group semantics don't map cleanly — punt (rare).
- **Renderer as test oracle, not dependency** (parallels stage 3's OTS
  gate): render corpus fixtures with poppler or pdf.js in CI, screenshot
  our HTML output, perceptual-diff — a corpus-driven convergence measure
  without shipping a renderer.

## Stage 5 — interaction & navigation

Builds on whatever pages already render; needs stage 0 plus destinations
from the page tree, little else.

- **Links**: URI actions and internal `GoTo` destinations (incl. named
  destinations) as `<a>` overlays — pdf2htmlEX does this and it reads as
  table stakes for "real" HTML.
- **Annotation appearances**: render `/AP` appearance streams (form
  XObjects again) for highlights, stamps and form-field appearances;
  AcroForm *interactivity* stays out of scope — read-only, like the rest
  of the engine.
- **Document outline** (`/Outlines`) → navigation anchors/sidebar; cheap
  once destinations resolve.
- **Optional content groups** (layers): honor default visibility when
  emitting; no toggle UI.
- **Metadata** (`/Info` dictionary, XMP) into `file_meta()`.
- **Output scaling**: one monolithic HTML vs. per-page lazy loading for
  large documents (pdf2htmlEX ships a small JS loader; check what odr's
  HTML service model already provides before inventing one).

## Cross-cutting (any time)

- Route diagnostics through `Logger` instead of stdout/stderr; drop the
  leftover debug code in `html/pdf_file.cpp`.
- Assertion-based tests per stage (the current tests are print-only smoke
  tests); grow a corpus: `odr-public` fixtures, the PDF101 "nasty files"
  collection linked in [`README.md`](README.md).
- Spec docs offline under `offline/documentation/PDF/` — **done** (ISO
  32000-1:2008, ISO 32000-2:2020 and the Adobe PDF Reference 1.7, with
  markdown conversions); still to do: fold them into `README.md` in place
  of the web links.
