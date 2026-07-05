# In-house PDF support (`pdf/`) — status, design & roadmap

What the `pdf/` module does **today**, the **design decisions** behind it, and
the **roadmap** for turning it into a faithful renderer. Reference links
(web resources; offline spec docs are planned) live in [`README.md`](README.md).

This is the `DecoderEngine::odr` path for PDF; the sibling `../pdf_poppler/`
module (poppler / pdf2htmlEX, behind `ODR_WITH_PDF2HTMLEX`) is the
production-quality alternative engine.

**Scope today.** Parse the PDF object/file structure (classic cross-reference
tables, cross-reference streams, object streams, hybrid files, with a
forward-scan recovery path for broken cross-references), build the page
tree with fonts and annotations, tokenize page content streams into graphics
operators, and emit an **HTML rendering**: absolutely positioned
text spans, one per shown segment, placed by the full text transform (CTM × text
matrix) and advanced by the parsed glyph widths, recursing into form XObjects,
with text render modes and `/ActualText` honoured and omitted spaces inferred
from glyph gaps, pages sized from `MediaBox`. Encrypted files are decrypted (RC4,
AES-128, AES-256). Embedded font programs (TrueType, CFF, Type1) render through
`@font-face`; non-embedded fonts are substituted to CSS `font-family` stacks
(with standard-14 AFM widths); and vector graphics, images, shadings, tiling
patterns, Type3 glyphs and transparency (constant alpha + blend modes) render as
inline SVG per page. Experimental and not production-quality.

---

## What works

- `.pdf` is detected by file magic and opened as `PdfFile`
  (`DecoderEngine::odr`); `is_decodable()` returns `false`. Page-tree/content
  parsing is lazy (on HTML request), but `file_meta()` carries a best-effort
  `document_meta`: the page count (root `/Pages /Count`, a single lookup) and
  the `/Info` document-information strings (Title/Author/Subject/Keywords/
  Creator/Producer/CreationDate/ModDate, decoded from UTF-16BE-BOM or
  PDFDocEncoding via `decode_text_string`), read once at construction (an
  owner-locked file is unlocked with the empty password first so its `/Info`
  decrypts). A malformed structure simply leaves `document_meta` minimal. XMP
  metadata is not yet parsed (see *Other known gaps*).
- **Object syntax**: null, booleans, integers/reals, names (incl. `#xx`
  escapes), literal strings (the `\n \r \t \b \f` control escapes, `\ddd`
  octal, escaped delimiters, and `\`-before-EOL line continuation — Table 3),
  hex strings, arrays, dictionaries, indirect references (`n g R`) —
  standalone and nested.
- **File structure**: header, `n g obj … endobj`, `stream` payloads (via
  `/Length`, with a scan-to-`endstream` fallback), classic `xref` tables,
  `trailer`, `startxref`, `%%EOF`; both sequential reading (`read_entry`) and
  random access via the xref table. **Incremental updates**: `startxref` found
  by scanning the file tail, then the `Prev` chain is followed (cycle-guarded),
  merging xref tables so the newest entry for each object wins.
- **Cross-reference streams, object streams, hybrid files** (PDF 1.5+): each
  trailer-chain section may be a classic table or a cross-reference stream
  (`/W`/`/Index`/`Size`, decoded via the filter framework, entry types 0/1/2;
  unknown types treated as absent). Xref entries are a tagged union
  (`FreeEntry`/`UsedEntry`/`CompressedEntry`); compressed objects are read from
  their object stream (`/N`/`/First` header, decoded once and cached per
  stream). Hybrid files follow the `XRefStm`-before-`Prev` lookup order.
  Lenient where the wild demands: `/Type /XRef` only warns, references to free
  or absent objects resolve to null with a `Logger` warning, `n g obj` need not
  end with a newline.
- **Cross-reference recovery**: when the trailer-chain walk throws (missing or
  garbage `startxref`, a broken `Prev` chain) or the document fails to build
  (no `/Root`, offsets pointing at the wrong objects), the whole file is
  forward-scanned for `n g obj` starts, rebuilding a synthetic xref (last
  definition of an id wins). `trailer` dictionaries are collected for `/Root`,
  `/Encrypt`, `/ID`; recovered `/Type /ObjStm` members are indexed as
  compressed entries; and, when no trailer supplied a `/Root`, a `/Type
  /Catalog` object is searched. Handles e.g. an HTTP response saved as `.pdf`
  (every offset shifted by the header).
- **Page tree**: `Catalog` → `Pages` (recursive) → `Page` with per-page
  `Resources` (fonts only) and `Annots` (raw dictionary only). Objects cached by
  reference (`DocumentParser::m_objects`).
- **Inherited page attributes**: the inheritable set per spec Table 30 —
  `Resources`, `MediaBox`, `CropBox`, `Rotate` — resolved by threading an
  accumulator down the `Pages` recursion (no `Parent` walk). Each `Page` carries
  the resolved `media_box`/`crop_box`/`rotate` and its resolved `resources`.
  Lenience: `CropBox` defaults to `MediaBox`, `Rotate` normalized to
  {0,90,180,270}, a `MediaBox` missing everywhere falls back to US Letter, a
  missing `Resources` to an empty dict — all with a `Logger` warning.
- **Stream filters** (`pdf_filter`): `/Filter` and `/DecodeParms` honoured,
  including chains and the inline-image abbreviations — FlateDecode and
  LZWDecode (both with TIFF and PNG predictors), ASCIIHexDecode, ASCII85Decode,
  RunLengthDecode. Image codecs (DCTDecode, JPXDecode, CCITTFaxDecode,
  JBIG2Decode) are deliberately not decoded: `decode()` stops and hands back the
  still-encoded payload for the image path; `read_decoded_stream` treats them as an
  error. The `Crypt` filter passes through only as `Identity`.
- **Encryption** (`pdf_encryption`): the standard security handler. An
  `Authenticator` parses `/Encrypt` and authenticates the password (user then
  owner; the empty password is tried first, so owner-locked files open
  transparently), producing a `Decryptor` that decrypts object strings and
  streams. RC4 (V 1/2, R 2/3, 40–128 bit),
  AES-128 crypt filters (V 4, R 4 — `StdCF` with `V2`/`AESV2`, `Identity`,
  honouring `StmF`/`StrF`) and AES-256 (V 5, R 6, AESV3) are all supported,
  including owner-only files and `EncryptMetadata false`. Streams are decrypted
  before `/Filter` decoding; cross-reference streams and object-stream members
  are left untouched. The user password is never retained: once `authenticate`
  succeeds, the derived key lives only inside the `Decryptor` (no accessor), and
  `PdfFile` carries the whole authenticated `Decryptor` forward — from the
  encryption probe to the render parse — so the HTML service unlocks the
  document without re-deriving the key. Permission bits (`/P`) are recorded, not
  enforced.
- **Fonts / text mapping**: a font's `ToUnicode` CMap stream is decoded and
  parsed. The `CMap` is multi-byte aware: `codespacerange` declares the code
  widths, `bfchar` and both `bfrange` forms (destination increment and explicit
  array) are applied, and destinations may be multi-character (ligatures).
  `translate_string` splits a string into codes of the codespace-declared width;
  an unmapped code passes through as its numeric value (identity for single
  bytes). When a simple font carries no `ToUnicode` CMap, `Font::to_unicode`
  falls back to its `/Encoding` — a base encoding (Standard/WinAnsi/MacRoman)
  overlaid with `/Differences`, each code → glyph name → Unicode via the Adobe
  Glyph List (incl. the `uniXXXX`/`uXXXXXX` forms). **Composite
  (Type0) fonts** are recognized: the descendant CIDFont's
  `/CIDSystemInfo` `/Registry`/`/Ordering` is recorded on the `Font`, and the
  Type0 `/Encoding` (a code → CID CMap such as `Identity-H`) is kept out of the
  simple-font encoding path. An *embedded* `/Encoding` CMap stream is parsed
  (`cidchar`/`cidrange` → `Font::cid_encoding`) so `Font::codes()` yields CIDs
  through it — this also carries the authoritative codespace, so a producer that
  mixes a 1-byte code (e.g. a space) among 2-byte CIDs stays aligned and selects
  the right glyph/advance. Extraction is driven by the `/ToUnicode` CMap (the
  common case — every Type0 font in the corpus carries one). When a composite
  font has no `/ToUnicode`, the named `/Encoding` still resolves the text
  (`pdf_cid`, backed by the generated `pdf_cid_data` tables): a **predefined
  Unicode `/Encoding`** — the `Uni*-UCS2/UTF16/UTF32` CMaps — is decoded directly
  (those codes already are Unicode, big-endian), and a **legacy CJK CMap**
  (`90ms-RKSJ-H`, `GBK-EUC-H`, `B5pc-H`, `KSC-EUC-H`, …) maps code → CID through
  its own codespace + range tables and then CID → Unicode through its character
  collection's table. When `/Encoding` is `Identity-H/V` (code == CID) or an
  embedded CMap stream (`Font::codes()` already yields the CIDs), the collection
  named by the descendant CIDFont's `/CIDSystemInfo` supplies the CID → Unicode
  step directly. Only a genuinely unmapped code (a CMap we ship no tables for, an
  `Identity` ordering, an unlisted CID) falls through to the embedded font's
  reverse map below, and failing that yields "no Unicode" (not byte-garbage).
- **Glyph metrics**: a font's advance widths are parsed —
  `/FirstChar` + `/Widths` + `/FontDescriptor` `/MissingWidth` for simple fonts,
  `/W` + `/DW` (the descendant CIDFont, both `c [w…]` and `c_first c_last w`
  forms) for composite fonts. `Font::advance_width(code)` returns the advance in
  text-space units (glyph-space / 1000), falling back to `/MissingWidth` or `/DW`.
  Codes outside the corpus are interpreted as CIDs for composite fonts (identity).
  A non-embedded standard-14 font (which ships no `/Widths`) falls back to a
  **generated AFM width table** (see *Graphics, images & transparency*).
- **Embedded font programs**: every embedded flavor is decoded into an
  `abstract::Font` held on `Font::embedded_font` — `/FontFile2` (TrueType, simple
  or composite `CIDFontType2`) → `SfntFont`; `/FontFile3` (CFF / `Type1C` /
  `CIDFontType0C`) → a bare `CffFont`, or an `SfntFont` when the program is a full
  OpenType SFNT; `/FontFile` (Type1) → translated to a CFF (`type1::to_cff`) and
  read as a `CffFont`, so it reuses the whole CFF path. An explicit `/CIDToGIDMap`
  stream is read into `Font::cid_to_gid` (empty = `Identity`). A malformed program
  is logged and leaves `embedded_font` null (fallback path). `Font::glyph_for_code(code)`
  maps a character code to a glyph id — composite: CID → GID via `/CIDToGIDMap`;
  simple TrueType: the embedded `cmap`, else the code's Unicode, else the code as a
  GID (best effort, ISO 32000-1 9.6.6.4). Two consumers read it: the HTML layer
  renders the actual glyphs (see *HTML*), and `Font::to_unicode` gains an
  **embedded-font reverse map** as its final fallback (code → glyph →
  `code_point_for_glyph`), so a font with neither a `/ToUnicode` CMap nor a usable
  `/Encoding` becomes selectable; a partially mapped run recovers what it can.
- **Content streams**: the full graphics-operator vocabulary is tokenized;
  `GraphicsState` executes a subset (state stack `q`/`Q`, matrices `cm`/`Tm`,
  line parameters, text state `Tc`/`Tw`/`Tz`/`TL`/`Tf`/`Tr`/`Ts`, glyph metrics
  `d0`/`d1`, grey/RGB/CMYK colors). The CTM **concatenates** on `cm` (ISO 32000-1
  8.4.4); the text matrix `Tm` and text line matrix `Tlm` are tracked as 2-D
  affine `Transform2D` values (`util/math_util.hpp`), with `BT` resetting them, `Td`/`TD`
  /`T*` (and the line-move half of `'`/`"`) advancing `Tlm` → `Tm`. Unknown
  operators are logged to stderr and skipped.
- **Text layout** (`pdf_page_text`): `extract_text` runs the
  operator parser + `GraphicsState` over a page's content and emits a
  renderer-agnostic `TextElement` per shown *segment* (one `Tj`/`'`/`"`, or one
  string of a `TJ` array) — its text-space → user-space transform (CTM × `Tm`,
  with horizontal scaling and rise folded in, font size kept separate), the
  resolved font, size, spacing parameters, raw codes, the CMap-translated
  Unicode, and the segment's per-code advances plus their total. Font lookup is
  lenient (unknown ref → warn, raw codes). **Glyph advances are applied**: after
  each segment the text matrix `Tm` advances by the glyph widths × font size plus
  char/word spacing (× horizontal scaling), and a `TJ` number translates `Tm` by
  `−n/1000 × Tfs × Th` — so segments, `TJ` kerning and lines land in the right
  place. The element carries the per-code advances directly, so a renderer wanting
  per-glyph placement need not re-derive them from `font->advance_width`. Each
  element also carries its text render mode (`Tr`) and a `no_unicode` flag — set
  when the font's code → Unicode chain yields nothing (a composite font with no
  `/ToUnicode` or usable predefined encoding), so `text` is empty and the run is
  knowingly non-extractable (its glyphs still render via the embedded program's
  PUA re-encode; see *HTML*).
  Marked content is tracked (`BMC`/`BDC … EMC`, balanced per stream and reset
  across form invocation): a sequence carrying `/ActualText` (inline property
  dictionary, or a name resolved through the `/Properties` resource) overrides the
  per-glyph Unicode of its enclosed shows — the decoded text (UTF-16BE BOM or
  PDFDocEncoding) emitted once and the rest of the sequence suppressed.
  **Space inference**: a running pen (the user-space origin after
  each segment, with the writing-line direction and em scale) is threaded through
  the executor, and a single space is prepended to a segment's `text` when the
  gap past the previous pen exceeds ~0.2 em along the line (a word break) or
  ~0.5 em perpendicular (a new line) — recovering the inter-word/-line spaces the
  producer omitted, in `text` only (codes/advances/placement untouched). Still
  deferred: vertical writing-mode advances (the deferred bidi & vertical writing
  work, see *Other known gaps*).
- **Form XObjects**: a resource dictionary's `/XObject`
  subdictionary is parsed into `Resources::x_object`; each `/Subtype /Form` is an
  `XObject` element carrying its `/Matrix` (default identity), its decoded
  content stream (read eagerly at parse time, so text extraction needs no parser
  handle), and its own parsed `/Resources` (or `nullptr` to inherit the invoking
  scope). `Do` in `extract_text` saves the state, concatenates the form `/Matrix`
  onto the CTM, runs the form content against the form's resources (falling back
  to the enclosing scope), then restores — so text inside forms is placed
  correctly. Image XObjects (`/Subtype /Image`) are decoded and emitted (see
  *Graphics, images & transparency*); unknown subtypes are inexecutable. The
  parser memoizes XObjects by
  reference (`ObjectReference -> XObject*`), so a form shared across pages is
  parsed once and a cyclic form reference resolves to the existing element — the
  in-memory graph mirrors the file, cycles included. A render-time active-set
  guard cuts cyclic invocation (the spec forbids it, 8.10.1, but real files
  contain it). `/BBox` clipping is deferred.
- **HTML**: one `document.html` view; each page is a `div` sized from `MediaBox`
  (points → inches). Each `TextElement` becomes an absolutely positioned `span`
  carrying a CSS `transform` matrix (the placement transform mapped from PDF user
  space — y-up, MediaBox origin — into the page box in CSS pixels, the glyphs
  un-mirrored so text stays upright), `font-size` from the text state, and the
  Unicode text. Invisible render modes (`Tr` 3/7) keep the span but paint it
  transparent (the `.i` class) so OCR-over-scan text stays selectable.
  **Embedded fonts**: a run whose font carries a usable program is
  emitted as a **dual layer** — a visible glyph layer (PUA code points in the
  font's `@font-face`, `aria-hidden` + `user-select:none`) overlaid by a
  transparent selectable layer carrying the real Unicode, so display is
  glyph-exact while copy/search read the Unicode (and the PUA code points never
  reach the clipboard). Each embedded program is re-encoded to the PUA once and
  wrapped into an OTF — after extraction, so the in-place re-encode can't disturb
  the reverse-map / glyph lookups that read the original `cmap` — and served as an
  inline `@font-face`;
  too many glyphs for the BMP PUA falls back to the default font. A run with no
  embedded program keeps the single-span fallback path; one with neither a
  program nor extractable text (a `no_unicode` run or an `/ActualText`-suppressed
  show) still emits no span.
- **Baseline placement.** PDF's text origin is the glyph *baseline*, but a CSS
  span anchors its box *top*, which sits one ascent above the baseline — so left
  uncorrected every run renders ~one ascent too low (highlights/underlines, which
  are painted correctly from user space, then float a line above their text).
  Each run is therefore raised by one font ascent so the baseline lands on the
  origin: in the uniform branch the shift comes off `top`
  (`top = (m.f - ascent_em·m.a·size)·pt_to_px`), in the general branch it goes
  *through* the matrix, subtracted along the local y axis `(c, d)` from the
  translation (it cannot be applied to `m.f` after the fact). The dual layer
  shares one shift: the nested PUA glyph layer is positioned relative to its
  parent, so placing the parent moves both.
  - **Why `line-height:1`.** The browser puts the first baseline at
    `top + half_leading + ascent`, with `ascent` read from the *rendering* font's
    `hhea`/`OS/2` and `half_leading` from `line-height`. `line-height:1` (on both
    `.t` and the nested-glyph `placement` constant) zeroes the half-leading band
    so the box-top→baseline distance is just the ascent; default `normal` would
    add an unknown offset. This is exact when ascent + descent ≈ 1 em, near so
    otherwise. We control the rendering metric: the re-encode synthesizes
    `OS/2`/`hhea` (`font/cff_transform.cpp` `serialize_os2`/`serialize_hhea`; the
    SFNT path passes the originals through), so our ascent can match the
    browser's.
  - **Non-embedded substitutes** render in a *local* system font whose
    `hhea`/`OS/2` we do **not** control, so the box-top→baseline distance would
    be that font's ascent, not our `ascent_em` — dropping e.g. a 120pt Times
    title well below its intended baseline. `SubstituteFontFaces` (in
    `pdf_file.cpp`) closes this by routing each substitute through a generated
    `@font-face` (`'odr-sN'`, `src: local(...)` of the family stack) carrying
    `ascent-override:ascent_em`, `descent-override:1−ascent_em`,
    `line-gap-override:0` — so the browser positions the baseline from *our*
    metric. Faces are deduped by (family, style, ascent); the family stack is
    kept after `'odr-sN'` as a fallback for the rare unresolved local.
  - **`ascent_em`** (in `pdf_file.cpp`): FontDescriptor `/Ascent`, else the
    embedded font's `bounding_box().y_max / units_per_em()`, else `0.8` em (which
    matches `serialize_os2`'s degenerate 0.8/0.2 fallback, so the fallback font
    and our math agree); clamped to `[0.5, 1.2]`. It is **font-format agnostic**
    — only `abstract::Font` virtuals (`bounding_box`, `units_per_em`) plus the
    PDF descriptor — so SFNT, bare-CFF, and Type1 (→CFF) all work with no
    per-subclass code.
  - **Refinements left open.** The bounding-box fallback is coarser than the
    font's designed ascender; an SFNT-specific `OS/2.sTypoAscender`/`hhea` read
    would match the browser better, but `/Ascent` is present and preferred in
    almost all real PDFs so the fallback rarely fires. When `/Ascent` and the
    embedded `OS/2` disagree, which to prefer (descriptor states intent, embedded
    matches rendering) is unsettled — currently `/Ascent` wins. The ascent is
    per-font, not per-CID. No focused unit test yet (verified visually on
    `style-various-1.pdf`); the reference-output snapshot test is the gate.
- **Link annotations** (`html/pdf_file.cpp`): a page's `/Link` annotations
  (ISO 32000-1 12.5.6.5) become absolutely-positioned `<a>` overlays. A `/URI`
  action is an external link (attribute-escaped `href`, opened in `_blank` via
  the document target); a `/GoTo` action or a direct `/Dest` is an internal
  `#pN` link — each page `div` carries a matching `id="pN"`. Destinations
  resolve a page *reference* (the first element of a `[page …]` array) to its
  index, including named destinations via the catalog `/Dests` dictionary and
  the `/Names /Dests` name tree (depth-guarded). The `/Rect` is mapped through
  the page transform to page-box points. Resolved entirely in the HTML layer
  from the parser's raw annotation dictionaries — no parser/IR change. Both
  render modes emit them. Deferred: remote/launch actions (`/GoToR`, `/Launch`),
  the destination's scroll position/zoom (only the target page is used), and
  non-`/Link` annotation appearances (see *Interaction & navigation*).

## Graphics, images & transparency

Vector and raster content renders as **inline SVG per page**, layered under the
text spans — serialization, not rasterization (**decision, 2026-06**: no native
renderer; pdf.js proves the full PDF graphics model needs none, and the PDF and
SVG imaging models are close cousins, so the mapping is mostly mechanical). The
rasterized-background fallback is rejected: it reintroduces the exact renderer
dependency the `odr` engine exists to avoid. Fidelity is bounded by operator
coverage rather than by a renderer's long tail; the reference-output test guards
it.

- **Paths**: `extract_page` produces a page-element IR (`PathElement`,
  `ImageElement`, `ShadingElement`, `TextElement`) in paint order; the executor
  resolves geometry through the CTM at construction, so the renderer maps it
  through the page transform alone. Fills (nonzero / even-odd) and strokes (width,
  caps, joins, miter limit, dash) → `<path>` with the matching SVG attributes; a
  zero-width stroke floors to a hairline.
- **Clipping** (`W`/`W*`): the current clip (an intersection of path regions) is
  snapshotted onto each painted element and installed as a per-page `<clipPath>`
  referenced by `clip-path` (deduplicated by geometry). Text is not yet clipped
  (see *Other known gaps*).
- **Colour spaces & functions**: DeviceGray/RGB/CMYK directly; CIE-based,
  ICCBased, Indexed, Separation/DeviceN and Lab resolve to RGB at emission time by
  sampling the tint `/Function` (types 0/2/3/4, evaluated by `pdf_function`). CMYK
  uses a naive (no-ICC) conversion; overprint is ignored.
- **Images**: `DCTDecode` JPEG pass-through → `<image>`; Flate/LZW rasters
  re-encoded as PNG; inline images (`BI`/`ID`/`EI`); `/ImageMask` stencils painted
  in the current fill colour; `/SMask` and `/Mask` (stencil + colour-key)
  composited into RGBA on the raster path (a mask on a JPEG base is left alone —
  decoding the JPEG to composite is out of scope). An image's `/ColorSpace` may
  be a device space, an inline array (Indexed/ICCBased/…), or a *name* resolved
  against the enclosing `/Resources /ColorSpace` table (the table is parsed
  before the `/XObject` table and threaded into image parsing).
- **Shadings** (axial type 2, radial type 3): `parse_shading` pre-samples the
  tint function into sRGB stops, so the renderer needs no evaluator. `sh` floods
  the clip (a `<rect>` filled with the gradient); a `/PatternType 2` shading
  pattern named by `scn` fills a path. Both emit
  `<linearGradient>`/`<radialGradient>` with `gradientUnits="userSpaceOnUse"`.
  `/Extend`/`/Background`/`/BBox` are parsed but not yet honoured (see gaps).
- **Tiling patterns** (`/PatternType 1`): the cell content stream runs as a mini
  page and becomes an SVG `<pattern>` tile repeated every `/XStep`/`/YStep`, with
  `patternTransform` placing the lattice; coloured (`/PaintType 1`) cells carry
  their own colours, uncoloured (`/PaintType 2`) cells paint in the current fill
  colour. Each cell is clipped to its `/BBox` (overlapping lattices and nested
  tile content are gaps; see gaps).
- **Non-embedded fonts**: a font with no `/FontFile*` is *substituted*, not
  rendered — `/BaseFont` + `/FontDescriptor` flags map to a CSS `font-family`
  fallback stack (serif/sans/mono, bold/italic), and the standard-14 metrics
  (which ship no `/Widths`) come from a **generated AFM width table**
  (`tools/pdf/generate_afm_data.py`, pinned to PDFBox). Glyph shapes are the
  browser's fallback font.
- **Type3 fonts**: the glyphs are `/CharProcs` content streams drawn in glyph
  space and mapped by `/FontMatrix`. Each glyph runs through the same graphics
  executor at `/FontMatrix × size × Tm × CTM`, so it paints as ordinary
  path/image elements; the shown run stays selectable (Unicode from the
  code → Unicode chain) but paints no visible text of its own (`render_as_graphics`,
  like an invisible `Tr 3` run). Recursion is depth-guarded.
- **Transparency**: `/ExtGState` constant alpha (`ca`/`CA`) →
  `fill-opacity`/`stroke-opacity`/`opacity`, blend modes (`/BM`) →
  `mix-blend-mode` (the 16 separable/non-separable modes map 1:1; unmappable names
  render normal), and soft masks (`/SMask`) → an SVG `<mask>`. All are part of the
  saved graphics state, so `q`/`Q` scope them like the CTM. A soft mask's
  transparency group `/G` is rendered by the extractor into a `SoftMask` (its
  content in user space); the HTML `MaskRegistry` serializes that into a
  `<mask>` (luminosity → the SVG luminance default, `/Alpha` → `mask-type`,
  `/BC` → a backdrop rect), and the painted element is wrapped in a masked `<g>`.
  A transparency group (`/Group` with `/S /Transparency`) is treated as a unit:
  the constant alpha, blend mode and soft mask in force when it is invoked fold
  into the group's output even when the group's own content resets them — the
  drop-shadow / faded-reflection idiom. Text under a soft mask is not yet masked
  (see gaps), and per-element folding approximates true group compositing where
  interior paints overlap.

The **reference-output snapshot test** (`test/data/reference-output/`) is the
graphics oracle: each change regenerates it and the diff is reviewed. (An
automated perceptual-diff gate is deferred; see gaps.)

## Module layout

| File (`pdf/`)                          | Role                                                  |
|----------------------------------------|-------------------------------------------------------|
| `pdf_object.{hpp,cpp}`                 | Object model: `Object` (`std::any`-based variant), `Array`, `Dictionary`, `Name`, `StandardString`/`HexString`, `ObjectReference`; `to_stream`/`to_string` dumping |
| `pdf_object_parser.{hpp,cpp}`          | Tokenizer over `std::streambuf`: whitespace/lines, numbers, names, strings, arrays, dictionaries, references |
| `pdf_file_object.{hpp,cpp}`            | File-structure entries: `Header`, `IndirectObject`, `Trailer`, `Xref` (tagged-union entries, `append`/`merge_hybrid`), `StartXref`, `Eof`, the `Entry` any-holder; `parse_xref_stream_table` and the `ObjectStream` payload wrapper |
| `pdf_file_parser.{hpp,cpp}`            | File-level reads on top of `ObjectParser`: indirect objects, xref, trailer, startxref, stream payloads, `seek_start_xref` |
| `pdf_filter.{hpp,cpp}`                 | Stream filter framework: `decode()` over the `/Filter`/`/DecodeParms` chain; ASCIIHex/ASCII85/LZW/Flate/RunLength decoders, TIFF/PNG predictors; image codecs returned undecoded (`DecodeResult::stopped_at_filter`) |
| `pdf_document_parser.{hpp,cpp}`        | `parse_document()`: xref/trailer chain → catalog → page tree; lazy object reads with cache; (deep) reference resolution; resources incl. the `/XObject` table, with an `ObjectReference → XObject*` cache that dedups shared forms and breaks cyclic form references; loads each font's embedded program (`/FontFile2` → `SfntFont`, `/FontFile3` → `CffFont`/`SfntFont`, `/FontFile` Type1 → `CffFont` via `type1::to_cff`) and `/CIDToGIDMap` |
| `pdf_encryption.{hpp,cpp}`             | Standard security handler: `Authenticator` (parse `/Encrypt`, authenticate password → `Decryptor`) and `Decryptor` (decrypt strings/streams; RC4, AES-128, AES-256), plus a `standard_security` namespace of pure key/password algorithms for known-answer tests |
| `pdf_document.hpp`                     | `Document`: arena of `Element`s + `catalog` pointer |
| `pdf_document_element.hpp`             | Element structs: `Catalog`, `Pages`, `Page`, `Annotation`, `Resources` (font + XObject + `/Properties` tables), `XObject` (Form/Image subtype, `/Matrix`, decoded content, own `/Resources`), `Font` (incl. the `composite`/`cid_registry`/`cid_ordering` Type0 facts, the `/Widths`-`/W`/`/DW` glyph metrics + `advance_width`, the `embedded_font`/`cid_to_gid` + `glyph_for_code`, and `to_unicode` with its embedded reverse-map fallback) |
| `pdf_cmap.{hpp,cpp}`                   | `CMap`: 1-byte glyph → UTF-16 `bfchar` map + string translation |
| `pdf_cmap_parser.{hpp,cpp}`            | `ToUnicode` CMap stream parser (`begincodespacerange` sets code widths; `beginbfchar` and both `beginbfrange` forms applied) |
| `pdf_encoding.{hpp,cpp}`               | Simple-font `/Encoding` → Unicode: `BaseEncoding` tables, `/Differences` overlay (`Encoding`), glyph-name → Unicode via AGL + `uniXXXX`/`uXXXXXX` |
| `pdf_cid.{hpp,cpp}`                    | Composite-font predefined `/Encoding` → Unicode: `Uni*-UCS2/UTF16/UTF32` decoded directly; legacy CJK CMaps mapped code → CID (interned-range binary search) → Unicode (collection bitmap rank); plus `cid_to_unicode(registry, ordering, cid)` for the `Identity-H/V` + embedded-stream path |
| `pdf_cid_data.{hpp,cpp}`              | **Generated** (`tools/pdf/generate_cid_data.py`): the S3-packed legacy-CMap tables — interned code → CID ranges + per-CMap `uint16` index lists, and a per-collection CID → Unicode presence bitmap + rank index + `uint16` value array (astral escape). ~0.58 MB compiled |
| `pdf_encoding_data.{hpp,cpp}`          | **Generated** (`tools/pdf/generate_encoding_data.py`): base-encoding tables + the Adobe Glyph List as a name-sorted array |
| `util/math_util.hpp`                   | `util::math::Transform2D`: 2-D affine transform (PDF row-vector convention) — compose, point-apply, translation/scaling factories |
| `pdf_graphics_operator.hpp`            | `GraphicsOperatorType` enum (full operator set) + `GraphicsOperator` (type + `Object` arguments) |
| `pdf_graphics_operator_parser.{hpp,cpp}` | Content-stream tokenizer: arguments then operator name |
| `pdf_graphics_state.{hpp,cpp}`         | `GraphicsState`: stack of `State` (general/path/text/color), `execute(op)` for the modelled subset; CTM/`Tm`/`Tlm` as `Transform2D`, `text_placement_matrix()` for the text rendering transform sans font size, `advance_text()` for the post-glyph `Tm` advance, `save()`/`restore()`/`concat_matrix()` reused by `q`/`Q`/`cm` and by form-XObject invocation |
| `pdf_page_text.{hpp,cpp}`             | `extract_text`: run the content stream through `GraphicsState`, emit a `TextElement` (placed transform + font/size/spacing + codes + Unicode + per-code advances + total advance + render mode + `no_unicode` flag) per shown segment, advancing `Tm` by the glyph widths and `TJ` adjustments; `Do` recurses into a form XObject (state save / `/Matrix` concat / scoped resources / restore) with an active-set cycle guard; a marked-content stack applies `/ActualText` and the no-Unicode marking; a running pen infers omitted inter-word/-line spaces |
| `pdf_file.{hpp,cpp}`                   | `abstract::PdfFile` wrapper; probes encryption at construction and implements `password_encrypted()`/`decrypt()`, carrying the authenticated `Decryptor` (not the password) so rendering needs no re-derivation |

Consumers outside the module: `open_strategy.cpp` (detection / engine
selection) and `html/pdf_file.cpp` (`create_pdf_service`; also the per-font PUA
re-encode + OTF wrap + `@font-face` and the dual-layer glyph/Unicode span
emission, using the `font/` module — `sfnt_*`, `cff_*`, `type1_*`).

## Pipeline: how a `.pdf` becomes HTML

1. **Wiring.** `open_strategy` maps `FileType::portable_document_format` to
   `PdfFile`; `DecoderEngine::poppler` (or the unknown-file-type fallback) can
   yield a `PopplerPdfFile` instead when built with `ODR_WITH_PDF2HTMLEX`.
   `html::translate(PdfFile)` picks the matching HTML service.
2. **Locate the xref.** `seek_start_xref` seeks to `EOF − 64`, scans for
   `startxref`; `read_start_xref` yields the most recent xref offset.
   (`read_header` exists but `parse_document` does not call it — the `%PDF-`
   header is only checked by magic detection earlier.)
3. **Walk the trailer chain.** `read_xref_section` dispatches: a classic table
   (`read_xref` + `read_trailer`) or a cross-reference stream (an indirect
   object whose dictionary doubles as the trailer dict; payload decoded via the
   filter framework, entries via `parse_xref_stream_table`). A trailer `XRefStm`
   (hybrid file) is read next and fills entries the classic table lacks or marks
   free (`merge_hybrid`). Sections merge into the accumulated table
   (`std::map::insert` keeps the first/newest entry), then `Prev` is followed
   (cycle-guarded). The first/newest trailer provides `Root`.
4. **Build the page tree.** `parse_catalog` → `parse_pages` recurses over
   `Kids` (dispatching on `Type`). Each `Page` keeps its raw dictionary, its
   `Contents` reference(s), parsed `Resources` (the `Font` table — each font's
   `ToUnicode` CMap parsed if present — and the `/XObject` table, with form
   XObjects' content/`/Matrix`/nested `/Resources` read eagerly and memoized by
   reference) and `Annots` (raw). `read_object`
   dispatches on the xref entry kind: used → seek + `read_indirect_object`;
   compressed → owning object stream decoded once, cached, member parsed from
   the cached payload; free/absent → null with a warning. Parsed objects cached
   by reference.
5. **Decode content.** Per page (depth-first), the `Contents` streams are read,
   decoded through their `/Filter` chain (`read_decoded_stream`), concatenated
   with a newline between streams.
6. **Lay out and emit.** `extract_text` runs `GraphicsOperatorParser` +
   `GraphicsState` over the content and returns a `TextElement` per shown
   segment, each placed by `text_placement_matrix()` (CTM × `Tm`, with horizontal
   scaling and rise folded in), its glyphs translated through the font's CMap.
   After each segment `Tm` is advanced by the glyph widths (`advance_width`) plus
   char/word spacing, and `TJ` numbers translate `Tm` directly, so segments and
   lines land correctly. `Do` recurses into a form XObject — state saved, the
   form `/Matrix` concatenated onto the CTM, the form content run against its own
   resources (or the enclosing scope), state restored — guarded against cyclic
   invocation. The HTML layer maps each element to a positioned `span`
   with a CSS `transform` (PDF user space → the page box in CSS pixels) and
   `font-size` from the text state. An embedded font program renders the real
   glyphs via `@font-face` (dual layer); a non-embedded font is substituted to a
   CSS `font-family` stack, and non-text content (paths, images, shadings,
   patterns, transparency) emits inline SVG.

---

## Design decisions

**Stream-based parsing with seeks, lazy object access.** Everything is parsed
off a `std::istream`/`std::streambuf` — no full-file buffer. Random access
(xref lookups, stream payloads) seeks; sequential tokenizing uses
single-character peek/bump (`geti`/`getc`/`bumpc`). Objects are parsed only when
referenced, and parsed `IndirectObject`s are cached by reference, so shared
objects are read once. Positions are `std::uint32_t` (files ≥ 4 GiB are out of
scope).

**`std::any`-based object model.** `Object` holds its value in `std::any` with
typed `is_*`/`as_*` accessors (mirrors `oldms/`'s `Entry`). Pro: one value type
throughout parser, document elements, and operator arguments. Con: no exhaustive
matching, RTTI lookups, and accidental copies are easy — `resolve_object_copy`
exists because rvalue access proved fiddly (see the `TODO why rvalue not
working?` in `pdf_document_parser.cpp`).

**References are recognized by lookahead.** `n g R` reads as plain integers
until the `R` appears, so a standalone `read_object` returns the *id* integer of
a reference; the enclosing context folds it in. `read_array` reacts when the `R`
token itself shows up (array elements may be bare adjacent integers, so it
cannot promote earlier); `read_dictionary` values and indirect-object bodies use
`promote_indirect_reference`, where a digit after the value can only be a `gen`
(the next token is otherwise a key, `>>`, or `endobj`). All rewind-free.

**Element tree as an arena.** `Document` owns all elements
(`vector<unique_ptr<Element>>`); `Catalog`/`Pages`/`Page`/… hold raw non-owning
pointers plus their original dictionary (`Element::object`), so unmodelled keys
stay inspectable. Navigation is by typed `is_<T>()`/`as_<T>()` accessors over
`kids` — thin `dynamic_cast` wrappers mirroring `Object`'s `is_*`/`as_*`
surface (the former `Type` tag enum was dropped in favour of RTTI).

**Fail early on malformed structure, tolerate unknown content.** Structural
surprises **throw** `std::runtime_error` (missing `obj`/`endobj`/`stream`/
`endstream`/`xref`/`startxref`, unexpected characters, an unknown page-tree
element type, stream exhaustion). A missing or unresolvable `/Length` is
tolerated — the stream extent is recovered by scanning to `endstream`. Unknown
**content** is tolerated: unrecognized operators logged and skipped, unmodelled
operators ignored by `execute`, annotations keep their raw dictionary. An
unmapped CMap code passes through as its numeric value (identity for single
bytes). References to free/absent
objects resolve to null with a warning; unknown xref-stream entry types treated
as absent (7.5.8.3). A structural throw in the cross-reference layer is not
fatal, though: it is caught once and the file is forward-scanned to rebuild the
table (*Cross-reference recovery* above) before giving up.

**Diagnostics route through `Logger`.** `DocumentParser` and `extract_text` take
a `Logger &` (default `Logger::null()`); no stray `stdout`/`stderr` prints remain
in the module — new diagnostics should follow the same pattern.

**Rendering is deferred to the browser; display and text are decoupled.** We emit
no rasterized output: glyphs render via the embedded font (`@font-face`) and
vector content via SVG — the browser draws everything. Because display
is driven by the *glyph*, not the extracted Unicode, **text-extraction gaps
degrade only selectability and search — never display.** A residual
code → Unicode case (a CMap we ship no tables for, an unlisted CID) still renders
correctly: all glyphs are re-encoded to the Private Use Area for display (the
uniform re-encode, decision 2026-06-19), with the extracted Unicode carried
separately to drive selection/search; runs with no recoverable Unicode are
additionally marked non-extractable (`user-select: none`, `aria-hidden`). So even
an unshipped-CMap CJK PDF looks right — only its text selectability is affected.
The common legacy-CJK selectability gap is now closed: the `pdf_cid_data` tables
resolve the predefined legacy CMaps and the `/CIDSystemInfo` collections (see
*Fonts / text mapping*).

**Font handling is in-house, facts-vs-glyphs split.** No FontForge (pdf.js proves
it unnecessary; it is a heavy build and no read-only library can inject the PUA
mappings we need). A thin `abstract::Font` interface exposes the *facts* every
consumer needs (glyph count, glyph → Unicode, advance widths, units-per-em, name,
bbox, symbolic flag) while glyph outlines pass through **byte-for-byte** (no
decompile/recompile); every embedded flavor — SFNT (`/FontFile2`), bare CFF
(`/FontFile3`), Type1 (`/FontFile` → CFF via `type1::to_cff`) — feeds the one
path. Display uses a **uniform PUA re-encode** (`U+E000 + glyph index`) over every
glyph, with the extracted Unicode carried separately for selection/search (see
*Rendering is deferred* above).

---

## Tests

- `test/src/internal/pdf/pdf_filter.cpp` — **assertion-based**, all inputs
  inline strings: every decoder, predictors, chains, image-codec stop,
  `Crypt`/unknown-filter errors.
- `test/src/internal/pdf/pdf_file_object.cpp` — **assertion-based**, inline
  only: cross-reference-stream entry decoding (field widths incl. 0, type
  default, big-endian fields, subsections, unknown types, error paths),
  `ObjectStream` header parsing and member lookup, `Xref::append` /
  `Xref::merge_hybrid` precedence.
- `test/src/internal/pdf/pdf_encryption.cpp` — **assertion-based**, inline
  vectors only: the standard security handler across R 2 (RC4-40), R 3
  (RC4-128), R 4 (AES-128/AESV2, incl. `EncryptMetadata false` and an
  owner-locked file) and R 6 (AES-256). Vectors come from the real fixtures and
  from `qpdf --encrypt` output frozen as literals — decrypting back to a known
  marker, so no test is circular and no fixture file ships.
  `crypto_util_test.cpp` covers the new MD5/RC4/SHA-384/512 primitives against
  public standard vectors.
- `test/src/internal/pdf/pdf_document_parser.cpp` — **assertion-based**
  whole-file tests over mini-PDFs assembled by the test-only
  `pdf_test_file_builder.{hpp,cpp}` (computes xref offsets/`startxref`, so tests
  show only the dictionaries; classic-table and uncompressed-xref-stream
  variants), plus inherited-page-attribute coverage (a multi-level `Pages` tree:
  per-page resolved `MediaBox`/`CropBox`/`Rotate`/`Resources`, override vs.
  inheritance, the `CropBox` ← `MediaBox` default, the missing-`MediaBox`
  US-Letter lenience), plus cross-reference-recovery coverage (inline broken
  mini-PDFs: garbage prepended, a bad `startxref`, no trailer at all → catalog
  scan, a duplicate id → last definition wins, a page tree living in an object
  stream), plus composite-font coverage (a Type0 font over an `Identity-H`
  descendant `CIDFontType2`: `composite`/`/CIDSystemInfo` recorded, 2-byte
  `/ToUnicode` extraction, the no-`/ToUnicode` "no Unicode" fallback, and a
  predefined `Uni*-UCS2-H` `/Encoding` extracting without a `/ToUnicode`), plus
  glyph-metric coverage (the composite `/W`+`/DW` and a simple
  `/FirstChar`/`/Widths`/`/MissingWidth` font, asserted through `advance_width`),
  plus form-XObject coverage: a cyclic `/Resources`
  reference (Fm0 → Fm1 → Fm0) terminating via the XObject cache and represented
  faithfully (the back-edge points at the same cached element), and a form shared
  by two pages parsed once. End-to-end: the classic fixture
  `odr-public/pdf/style-various-1.pdf`, plus decryption of
  `odr-public/pdf/Casio_WVA-M650-7AJF.pdf` (RC4, empty password) and
  `odr-private/pdf/encrypted_fontfile3_opentype.pdf` (AES-256; skipped when the
  private submodule is absent), and recovery of the real
  `odr-private/pdf/order-EK52VKL0.pdf` (an HTTP response saved as `.pdf`;
  likewise skipped when absent). The `odr-private` xref-stream/objstm/hybrid
  fixtures (`basic_text.pdf`, `geneve_1564.pdf`, `test_fail.pdf`, `Kayla….pdf`,
  `svg_background…issue402.pdf`, `Core_v5.1.pdf`, `onepage.pdf`) were verified
  manually but are not pinned in unit tests. Also still contains the original
  print-everything smoke test.
- `test/src/internal/pdf/pdf_file_parser.cpp` — sequential `read_entry` walk
  (smoke) + assertion-based xref/trailer/root navigation over
  `style-various-1.pdf`.
- `test/src/internal/pdf/pdf_cmap.cpp` — **assertion-based**, inline CMap
  strings parsed through `CMapParser`: single- and two-byte `bfchar`, both
  `bfrange` forms, multi-character (ligature) targets, the identity fallback for
  unmapped codes, and mixed code widths driven by `codespacerange`.
- `test/src/internal/pdf/pdf_encoding.cpp` — **assertion-based**, no fixtures:
  `base_encoding_from_name`, glyph-name → Unicode via the AGL (the `fi` ligature,
  a multi-code-point decomposition, and the `name.suffix` form) and the
  algorithmic `uniXXXX`/`uXXXXXX` forms, `Encoding::translate_string` with a base
  encoding, the Latin-1 upper half (WinAnsi/MacRoman), a `/Differences` override,
  and the WinAnsi-vs-Standard `0x27` divergence.
- `test/src/internal/pdf/pdf_cid.cpp` — **assertion-based**, no fixtures (the
  vectors are derived from the Adobe CMap data by `tools/pdf` and frozen inline):
  `translate_predefined_cmap` over the predefined Unicode CMaps — `UCS2`/`UTF16`
  (incl. a surrogate pair) and `UTF32` decoding, a `-V` writing-mode variant — and
  over the legacy CJK CMaps — a Shift-JIS (`90ms-RKSJ-H`, Adobe-Japan1), an EUC
  (`GBK-EUC-H`, Adobe-GB1) and a Hojo (`Hojo-H`, deprecated Adobe-Japan2) `code →
  CID → Unicode`, plus a mixed 1-/2-byte codespace staying aligned; `cid_to_unicode`
  by `/CIDSystemInfo` collection (`Adobe`/`Japan1`, `Manga1` and `Japan2`, incl.
  an astral escape and the unknown-collection/CID `nullopt`); and the `nullopt`
  for `Identity-H` and an unshipped name.
- `test/src/internal/util/math_util_test.cpp` — **assertion-based**, no fixtures:
  `Transform2D` point-apply (identity/translation/scaling), the ordered
  (row-vector) composition, and compose-then-apply ≡ sequential apply.
- `test/src/internal/pdf/pdf_page_text.cpp` — **assertion-based**, inline content
  streams through `extract_text`: `Td` translation, `Tm` scaling, `cm` CTM
  concatenation under `Tm`, horizontal scaling and rise in the transform, and the
  `T*`/`'`/`"` line moves with their leading and spacing; plus
  glyph-advance coverage with hand-built `Font`s — simple `/Widths` advancing a
  following show, `TJ` emitting per string with the numeric adjustment applied,
  char spacing, word spacing on the single-byte space, the composite 2-byte `/DW`
  advance, and the `advance_width` fallbacks; plus form-XObject
  coverage with hand-built `XObject`s — invocation via `Do`, the
  `/Matrix` placement, state restoration after the form, the form's own
  `/Resources` scope, nested forms, image/unknown XObjects ignored, and a
  self-referential form terminating at render time via the active-set guard;
  plus render-mode and extraction-refinement coverage — `Tr`
  propagation, a composite font with no `/ToUnicode` marked `no_unicode` with
  empty text, and `/ActualText` overriding a segment (literal and UTF-16BE),
  emitted once then suppressed across the sequence, resolved via a named
  `/Properties` entry, the no-`/ActualText` passthrough, and a tolerated stray
  `EMC`; plus space-inference coverage — a word-gap space, no space
  when segments abut, the `TJ`-kern threshold (sub- vs. supra-threshold), a
  new-line space, and the no-double-space after a trailing space.
- `test/src/internal/pdf/pdf_font_program.cpp` — **assertion-based**, no
  fixtures: hand-built `Font`s over tiny in-memory `SfntFont`s (a compact SFNT
  builder mapping a code run to glyph ids). `glyph_for_code` — composite
  `Identity`, an explicit `/CIDToGIDMap` (incl. an out-of-range CID → `.notdef`),
  a simple-font code reaching its glyph through the embedded `(3,1)` `cmap`, and
  the no-program zero — and the embedded reverse-map `to_unicode`: recovery when
  there is no `/ToUnicode` (with an unreachable glyph staying unmapped) and a
  `/ToUnicode` CMap taking precedence over the reverse map.

The tokenizer's string parsing is covered (`PdfObjectParser`: literal-string
control/octal/delimiter/line-continuation escapes and hex strings); references
and the HTML output itself (the span emission / CSS transform mapping, incl. the
dual-layer glyph/Unicode emission) are not yet asserted.

---

# Roadmap

Goal: faithful read-only HTML for common real-world PDFs through the odr engine,
so the poppler/pdf2htmlEX engine becomes optional rather than required. The
file-format, text-extraction, font and graphics foundations are in place (see
*What works* and *Graphics, images & transparency*); what remains is interaction
& navigation plus a tail of known gaps. Each remaining item gets its own detailed
design before implementation.

## Interaction & navigation

The next feature cluster — needs destinations from the page tree, little else.
Link annotations (`/URI` + internal `/GoTo`) already land — see *What works*.

- **Annotation appearances**: render `/AP` appearance streams (form XObjects
  again) for highlights, stamps, form-field appearances; AcroForm
  *interactivity* stays out of scope (read-only).
- **Remote/launch link actions** (`/GoToR`, `/Launch`) and destination scroll
  position/zoom (the internal-link handler uses only the target page).
- **Link overlays vs. text selection**: the `<a>` overlays sit above the `.sel`
  text, so a mousedown over a link starts navigation instead of a selection —
  links are clickable but text under them cannot be selected, and there is no
  hover cursor issue since native `:hover` applies. A JS workaround existed
  (overlays `pointer-events:none` so mousedown falls through to `.sel`; a
  once-per-document click handler re-enabled hit-testing via an `.lkhit` class,
  `elementFromPoint`, and forwarded the click; a rAF-throttled `mousemove`
  restored the pointer cursor via an `.lkc` class) but was rolled back as too
  involved (reverted commit `5cfa8a09`, reachable for later). Options to
  revisit: (a) reinstate that JS approach; (b) a CSS-only route if one exists;
  (c) leave overlays clickable-only and accept no under-link selection.
- **Document outline** (`/Outlines`) → navigation anchors/sidebar.
- **Optional content groups** (layers): honor default visibility; no toggle UI.
- **Output scaling**: monolithic HTML vs. per-page lazy loading for large
  documents (check what odr's HTML service model already provides first).

## Cross-cutting (any time)

- Grow a corpus: `odr-public` fixtures, the PDF101 "nasty files" collection
  linked in `README.md`; assertion-based tests per feature.
- Spec docs offline under `offline/documentation/PDF/` (ISO 32000-1:2008, ISO
  32000-2:2020, Adobe PDF Reference 1.7, with markdown conversions); still to
  do: fold them into `README.md` in place of the web links.

## Other known gaps

- **Graphics deferrals** (from stage 4, the *Graphics, images & transparency*
  tail):
  - **Soft masks** (`/SMask` in `/ExtGState` → SVG `<mask>`): done for
    luminosity and alpha masks on graphic content (see *Transparency* above).
    Remaining gaps: a soft mask (or group opacity/blend) on *text* is not applied
    — the visible glyphs live in the HTML text layer, not the SVG, so the mask
    would need a CSS `mask` on the line block or the run promoted into SVG (the
    masked content in practice is paths/images, not text). Isolated vs.
    non-isolated and knockout group semantics (`/I`, `/K`) are not distinguished;
    a soft mask's own group is rendered with its default (black) backdrop unless
    `/BC` is a plain device colour.
  - **Mesh & function-based shadings** (types 1, 4–7): only axial (2) and radial
    (3) are emitted; the rest would tessellate into flat polygons (pdf.js's
    approach) — not done.
  - **Non-extended shadings**: `/Extend`/`/Background`/`/BBox` are parsed onto
    `Shading` but not honoured — the renderer always uses SVG's `pad` spread, so a
    non-extended shading over-paints past its interval (needs the fill clipped to
    the gradient band/annulus).
  - **Overlapping tiling lattices** (a `/PatternType 1` step smaller than the
    `/BBox`) can't be expressed as one SVG `<pattern>` and are not reproduced;
    nested text/shadings/patterns inside a tile are skipped (rare).
  - **Text clipping**: the clip is not applied to text runs (paths and images are
    clipped); text clip render modes (`Tr` 4–7) add nothing to the clip.
  - **Perceptual-diff oracle**: the reference-output snapshot test is the current
    graphics gate; the automated poppler/pdf.js screenshot-diff is not built.
- **Encryption edge cases** (deferred from stage 0 until a real file needs
  them): per-stream `/Crypt` filter `Name` overrides, the `EncryptMetadata
  false` metadata-stream `Identity` special case, and `Perms` (Algorithm 13)
  validation. The public-key security handler and revision 5 are out of scope.
- **Recovery limitations** (deferred from stage 0): when several `/Type
  /Catalog` objects survive, the first in id order is picked rather than the
  newest; the constructor-triggered recovery path cannot decode object streams
  in an *encrypted* broken file (no decryptor yet), so such members go
  unindexed. Both are edge cases beyond the corpus seen so far.
- **Linearized files** are not handled specially (the tail-first read usually
  still works, but hint streams are ignored).
- **Bidi & vertical writing** (deferred): RTL run reordering for the
  layout/selection order, and vertical writing mode (`Identity-V`/CJK — the
  `/W2`/`/DW2` vertical metrics and a perpendicular pen advance, which the
  horizontal-only `extract_text` and space inference assume away). No corpus
  fixture needs either yet; revisit when one does.
- **Annotations**: `/Link` annotations render as `<a>` overlays (see *What
  works*); other annotation types are collected but their appearance streams are
  not interpreted (*Interaction & navigation*).
- **XMP metadata** (`/Metadata` XML packet) is not parsed; `file_meta()`'s
  `document_meta` comes from the `/Info` dictionary only. XMP would supplement
  or override `/Info` for producers that write it.
