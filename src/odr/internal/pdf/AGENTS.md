# In-house PDF support (`pdf/`) — status, design & roadmap

What the `pdf/` module does **today**, the **design decisions** behind it, and
the **staged roadmap** for turning it into a faithful renderer. Reference links
(web resources; offline spec docs are planned) live in [`README.md`](README.md).

This is the `DecoderEngine::odr` path for PDF; the sibling `../pdf_poppler/`
module (poppler / pdf2htmlEX, behind `ODR_WITH_PDF2HTMLEX`) is the
production-quality alternative engine.

**Scope today.** Parse the PDF object/file structure (classic cross-reference
tables, cross-reference streams, object streams, hybrid files, with a
forward-scan recovery path for broken cross-references), build the page
tree with fonts and annotations, tokenize page content streams into graphics
operators, and emit a **proof-of-concept HTML rendering**: absolutely positioned
text spans, one per shown segment, placed by the full text transform (CTM × text
matrix) and advanced by the parsed glyph widths, recursing into form XObjects,
with text render modes and `/ActualText` honoured and omitted spaces inferred
from glyph gaps, pages sized from `MediaBox`. Encrypted files are decrypted (RC4,
AES-128, AES-256). Embedded TrueType fonts render through `@font-face` (other
font flavors fall back to a system font); no graphics, no images yet.
Experimental and not production-quality.

---

## What works

- `.pdf` is detected by file magic and opened as `PdfFile`
  (`DecoderEngine::odr`); `is_decodable()` returns `false` and `file_meta()`
  carries only the file type. All parsing is lazy, on HTML request.
- **Object syntax**: null, booleans, integers/reals, names (incl. `#xx`
  escapes), literal strings (`\` and `\ooo` escapes), hex strings, arrays,
  dictionaries, indirect references (`n g R`) — standalone and nested.
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
  still-encoded payload for stage 4; `read_decoded_stream` treats them as an
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
  simple-font encoding path. Extraction is driven by the `/ToUnicode` CMap (the
  common case — every Type0 font in the corpus carries one). When a composite
  font has no `/ToUnicode`, a **predefined Unicode `/Encoding`** — the
  `Uni*-UCS2/UTF16/UTF32` CMaps — is decoded directly (`pdf_cid`), since those
  character codes already are Unicode (big-endian); any
  other case (`Identity-H/V`, or the legacy CJK code→CID CMaps) yields "no
  Unicode" (not byte-garbage) until the legacy CID → Unicode tables land or —
  for an embedded TrueType program — the reverse map below recovers it (stage
  3.3).
- **Glyph metrics**: a font's advance widths are parsed —
  `/FirstChar` + `/Widths` + `/FontDescriptor` `/MissingWidth` for simple fonts,
  `/W` + `/DW` (the descendant CIDFont, both `c [w…]` and `c_first c_last w`
  forms) for composite fonts. `Font::advance_width(code)` returns the advance in
  text-space units (glyph-space / 1000), falling back to `/MissingWidth` or `/DW`.
  Codes outside the corpus are interpreted as CIDs for composite fonts (identity);
  AFM widths for the non-embedded standard-14 fonts are stage 3.
- **Embedded font programs** (stage 3.3): a TrueType `/FontFile2` (a simple font,
  or a composite `CIDFontType2`) is decoded into an `abstract::Font` (`SfntFont`)
  and held on `Font::embedded_font`; an explicit `/CIDToGIDMap` stream is read into
  `Font::cid_to_gid` (empty = `Identity`). `Font::glyph_for_code(code)` maps a
  character code to a glyph id — composite: CID → GID via `/CIDToGIDMap`; simple
  TrueType: the embedded `cmap`, else the code's Unicode, else the code as a GID
  (best effort, ISO 32000-1 9.6.6.4). Two consumers read it: the HTML layer
  renders the actual glyphs (see *HTML*), and `Font::to_unicode` gains an
  **embedded-font reverse map** as its final fallback (code → glyph →
  `code_point_for_glyph`), so a font with neither a `/ToUnicode` CMap nor a
  usable `/Encoding` becomes selectable — closing the stage-1 extraction gap for
  these fonts; a partially mapped run recovers what it can. CFF (`/FontFile3`)
  and Type1 (`/FontFile`) leave `embedded_font` null (stages 3.4 / 3.5).
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
  knowingly non-extractable until stage 3 re-encodes the glyphs to the PUA.
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
  deferred: intra-segment glyph shaping (the browser lays a segment out in a
  fallback font until stage 3) and vertical writing-mode advances (the deferred
  bidi & vertical writing work, see *Other known gaps*).
- **Form XObjects**: a resource dictionary's `/XObject`
  subdictionary is parsed into `Resources::x_object`; each `/Subtype /Form` is an
  `XObject` element carrying its `/Matrix` (default identity), its decoded
  content stream (read eagerly at parse time, so text extraction needs no parser
  handle), and its own parsed `/Resources` (or `nullptr` to inherit the invoking
  scope). `Do` in `extract_text` saves the state, concatenates the form `/Matrix`
  onto the CTM, runs the form content against the form's resources (falling back
  to the enclosing scope), then restores — so text inside forms is placed
  correctly. Image XObjects (`/Subtype /Image`) are recognized but not rendered
  (stage 4); unknown subtypes are inexecutable. The parser memoizes XObjects by
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
  **Embedded fonts** (stage 3.3): a run whose font carries a usable program is
  emitted as a **dual layer** — a visible glyph layer (PUA code points in the
  font's `@font-face`, `aria-hidden` + `user-select:none`) overlaid by a
  transparent selectable layer carrying the real Unicode, so display is
  glyph-exact while copy/search read the Unicode (and the PUA code points never
  reach the clipboard). Each embedded SFNT is re-encoded to the PUA once — after
  extraction, so the in-place re-encode can't disturb the reverse-map / glyph
  lookups that read the original `cmap` — and served as an inline `@font-face`;
  too many glyphs for the BMP PUA falls back to the default font. A run with no
  embedded program keeps the single-span fallback path; one with neither a
  program nor extractable text (a `no_unicode` run or an `/ActualText`-suppressed
  show) still emits no span. Precise baseline placement (needs font ascent
  metrics) is deferred; the baseline currently sits at the span's box top.

## Module layout

| File (`pdf/`)                          | Role                                                  |
|----------------------------------------|-------------------------------------------------------|
| `pdf_object.{hpp,cpp}`                 | Object model: `Object` (`std::any`-based variant), `Array`, `Dictionary`, `Name`, `StandardString`/`HexString`, `ObjectReference`; `to_stream`/`to_string` dumping |
| `pdf_object_parser.{hpp,cpp}`          | Tokenizer over `std::streambuf`: whitespace/lines, numbers, names, strings, arrays, dictionaries, references |
| `pdf_file_object.{hpp,cpp}`            | File-structure entries: `Header`, `IndirectObject`, `Trailer`, `Xref` (tagged-union entries, `append`/`merge_hybrid`), `StartXref`, `Eof`, the `Entry` any-holder; `parse_xref_stream_table` and the `ObjectStream` payload wrapper |
| `pdf_file_parser.{hpp,cpp}`            | File-level reads on top of `ObjectParser`: indirect objects, xref, trailer, startxref, stream payloads, `seek_start_xref` |
| `pdf_filter.{hpp,cpp}`                 | Stream filter framework: `decode()` over the `/Filter`/`/DecodeParms` chain; ASCIIHex/ASCII85/LZW/Flate/RunLength decoders, TIFF/PNG predictors; image codecs returned undecoded (`DecodeResult::stopped_at_filter`) |
| `pdf_document_parser.{hpp,cpp}`        | `parse_document()`: xref/trailer chain → catalog → page tree; lazy object reads with cache; (deep) reference resolution; resources incl. the `/XObject` table, with an `ObjectReference → XObject*` cache that dedups shared forms and breaks cyclic form references; loads each font's embedded program (`/FontFile2` → `SfntFont`) and `/CIDToGIDMap` (stage 3.3) |
| `pdf_encryption.{hpp,cpp}`             | Standard security handler: `Authenticator` (parse `/Encrypt`, authenticate password → `Decryptor`) and `Decryptor` (decrypt strings/streams; RC4, AES-128, AES-256), plus a `standard_security` namespace of pure key/password algorithms for known-answer tests |
| `pdf_document.hpp`                     | `Document`: arena of `Element`s + `catalog` pointer |
| `pdf_document_element.hpp`             | Element structs: `Catalog`, `Pages`, `Page`, `Annotation`, `Resources` (font + XObject + `/Properties` tables), `XObject` (Form/Image subtype, `/Matrix`, decoded content, own `/Resources`), `Font` (incl. the `composite`/`cid_registry`/`cid_ordering` Type0 facts, the `/Widths`-`/W`/`/DW` glyph metrics + `advance_width`, the `embedded_font`/`cid_to_gid` + `glyph_for_code`, and `to_unicode` with its embedded reverse-map fallback) |
| `pdf_cmap.{hpp,cpp}`                   | `CMap`: 1-byte glyph → UTF-16 `bfchar` map + string translation |
| `pdf_cmap_parser.{hpp,cpp}`            | `ToUnicode` CMap stream parser (`begincodespacerange`/`beginbfchar`/`beginbfrange`; only `bfchar` applied) |
| `pdf_encoding.{hpp,cpp}`               | Simple-font `/Encoding` → Unicode: `BaseEncoding` tables, `/Differences` overlay (`Encoding`), glyph-name → Unicode via AGL + `uniXXXX`/`uXXXXXX` |
| `pdf_cid.{hpp,cpp}`                    | Composite-font predefined `/Encoding` → Unicode: the `Uni*-UCS2/UTF16/UTF32` CMaps decoded directly (no data tables); legacy CJK CMaps deferred (see `tools/pdf/generate_cid_data.py`) |
| `pdf_encoding_data.{hpp,cpp}`          | **Generated** (`tools/pdf/generate_encoding_data.py`): base-encoding tables + the Adobe Glyph List as a name-sorted array |
| `util/math_util.hpp`                   | `util::math::Transform2D`: 2-D affine transform (PDF row-vector convention) — compose, point-apply, translation/scaling factories |
| `pdf_graphics_operator.hpp`            | `GraphicsOperatorType` enum (full operator set) + `GraphicsOperator` (type + `Object` arguments) |
| `pdf_graphics_operator_parser.{hpp,cpp}` | Content-stream tokenizer: arguments then operator name |
| `pdf_graphics_state.{hpp,cpp}`         | `GraphicsState`: stack of `State` (general/path/text/color), `execute(op)` for the modelled subset; CTM/`Tm`/`Tlm` as `Transform2D`, `text_placement_matrix()` for the text rendering transform sans font size, `advance_text()` for the post-glyph `Tm` advance, `save()`/`restore()`/`concat_matrix()` reused by `q`/`Q`/`cm` and by form-XObject invocation |
| `pdf_page_text.{hpp,cpp}`             | `extract_text`: run the content stream through `GraphicsState`, emit a `TextElement` (placed transform + font/size/spacing + codes + Unicode + per-code advances + total advance + render mode + `no_unicode` flag) per shown segment, advancing `Tm` by the glyph widths and `TJ` adjustments; `Do` recurses into a form XObject (state save / `/Matrix` concat / scoped resources / restore) with an active-set cycle guard; a marked-content stack applies `/ActualText` and the no-Unicode marking; a running pen infers omitted inter-word/-line spaces |
| `pdf_file.{hpp,cpp}`                   | `abstract::PdfFile` wrapper; probes encryption at construction and implements `password_encrypted()`/`decrypt()`, carrying the authenticated `Decryptor` (not the password) so rendering needs no re-derivation |

Consumers outside the module: `open_strategy.cpp` (detection / engine
selection) and `html/pdf_file.cpp` (`create_pdf_service`; also the stage-3.3
per-font PUA re-encode + `@font-face` and the dual-layer glyph/Unicode span
emission, using `font/sfnt_*`).

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
   `font-size` from the text state. Intra-segment glyph shaping is the browser's
   until the embedded font lands (stage 3).

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

**References are recognized by lookahead.** `n g R` is plain integers until the
`R` appears, so `read_array`/`read_dictionary` patch references after the fact.
A standalone `read_object` therefore returns the *id* integer of a reference —
only array/dictionary contexts and `read_object_reference` assemble real
references. Works for well-formed files; a known sharp edge (`TODO this seems
hacky`).

**Element tree as an arena.** `Document` owns all elements
(`vector<unique_ptr<Element>>`); `Catalog`/`Pages`/`Page`/… hold raw non-owning
pointers plus their original dictionary (`Element::object`), so unmodelled keys
stay inspectable. Navigation is by typed `is_<T>()`/`as_<T>()` accessors over
`kids` — thin `dynamic_cast` wrappers mirroring `Object`'s `is_*`/`as_*`
surface (the former `Type` tag enum was dropped in favour of RTTI).

**Fail early on malformed structure, tolerate unknown content.** Structural
surprises **throw** `std::runtime_error` (missing `obj`/`endobj`/`stream`/
`endstream`/`xref`/`startxref`, unexpected characters, an unresolvable
`/Length`, an unknown page-tree element type, stream exhaustion). Unknown
**content** is tolerated: unrecognized operators logged and skipped, unmodelled
operators ignored by `execute`, annotations keep their raw dictionary, CMap
`codespacerange`/`bfrange` parsed past without effect. References to free/absent
objects resolve to null with a warning; unknown xref-stream entry types treated
as absent (7.5.8.3). A structural throw in the cross-reference layer is not
fatal, though: it is caught once and the file is forward-scanned to rebuild the
table (*Cross-reference recovery* above) before giving up.

**Debug output still in place.** `pdf_graphics_state.cpp` (dash pattern, stroke/
other color) and `pdf_graphics_operator_parser.cpp` still print diagnostics to
stdout/stderr instead of `Logger`. The text path is clean: `html/pdf_file.cpp`
and `pdf_page_text.cpp` route through `Logger`. `DocumentParser` and
`extract_text` take a `Logger &` (default
`Logger::null()`) — new diagnostics should do the same.

**Rendering is deferred to the browser; display and text are decoupled.** We emit
no rasterized output: glyphs render via the embedded font (`@font-face`, stage 3)
and vector content via SVG (stage 4) — the browser draws everything. Because
display is driven by the *glyph*, not the extracted Unicode, **text-extraction
gaps degrade only selectability and search — never display.** Every deferred
code → Unicode case (legacy CJK CID → Unicode tables, `Identity-H` without
`/ToUnicode`, the remaining CFF/Type1 reverse maps) still renders correctly once stage 3 lands:
all glyphs are re-encoded to the Private Use Area for display (stage 3's uniform
re-encode, decision 2026-06-19), with the extracted Unicode carried separately to
drive selection/search; runs with no recoverable Unicode are additionally marked
non-extractable (`user-select: none`, `aria-hidden`). So the
deferred CJK/legacy-CMap work is a *selectability* gap, not a rendering risk —
such PDFs look right, their text just isn't selectable until the tables land.

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
- `test/src/internal/pdf/pdf_cid.cpp` — **assertion-based**, no fixtures:
  `translate_predefined_cmap` over the predefined Unicode CMaps — `UCS2`/`UTF16`
  (incl. a surrogate pair) and `UTF32` decoding, a `-V` writing-mode variant, and
  the `nullopt` for `Identity-H` and the legacy CJK CMaps.
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

No assertion-based coverage of the tokenizer (escapes, references, hex strings)
or the HTML output itself (the span emission / CSS transform mapping, incl. the
stage-3.3 dual layer).

---

# Roadmap

Goal: faithful read-only HTML for common real-world PDFs through the odr engine,
so the poppler/pdf2htmlEX engine becomes optional rather than required. Stages
are ordered by what they unlock; 0–2 are roughly sequential, 3 and 4 are
independent, 5 builds on whatever pages already render. Each stage gets its own
detailed design before implementation.

## Stage 0 — file-format compatibility (prerequisite) — **done**

The prerequisite for everything below: read the structures modern producers
write that the original parser rejected, so a real-world `.pdf` reaches the page
tree at all. All of it has landed (see *What works*): the stream-filter
framework (incl. PNG predictors), PDF 1.5+ cross-reference/object streams and
hybrid files, inherited page attributes, encryption (RC4 / AES-128 / AES-256),
and last-resort cross-reference recovery for broken files. Remaining odds and
ends are folded into *Other known gaps* below; the staged renderer work now
builds on a parser that opens the common corpus.

## Stage 1 — text extraction: the code → Unicode chain — **done**

**Goal.** PDF strings are **character codes**; per font, walk code → Unicode and
record it per code (or "no Unicode", which stage 3 re-encodes). The landed work
covers the local corpus and the bulk of real-world PDFs. The mechanics live in
*Fonts / text mapping* under *What works*; this is the summary.

**Achieved**
- **`ToUnicode` CMap.** Multi-byte codes (codespace-driven chunking), both
  `bfrange` forms, multi-character (ligature) targets.
- **Simple-font `/Encoding`.** Standard/WinAnsi/MacRoman base encodings +
  `/Differences` → glyph name → Unicode via the generated Adobe Glyph List (incl.
  the algorithmic `uniXXXX`/`uXXXXXX` forms).
- **Composite (Type0/CID) fonts.** Type0 structure + `/CIDSystemInfo`,
  `Identity-H/V`, `/ToUnicode`-driven extraction, and the predefined Unicode
  CMaps (`Uni*-UCS2/UTF16/UTF32`, decoded directly with no data tables).
- **"No Unicode" run marking + `/ActualText`** landed with stage 2 (run/state
  plumbing); see *Text layout* under *What works*.

**Deferred — relocated to later stages.** None blocks the renderer and no corpus
fixture needs them yet:
- **Legacy CJK code→CID CMaps** (RKSJ/EUC/Big5/GBK/KSC) + their `CID → Unicode`
  tables — large external data; the generator/fetch scaffolding is landed
  (`tools/pdf/generate_cid_data.py`), the storage decision and C++ lookup remain.
  Tracked under *Other known gaps* (CMap coverage).
- **Embedded-font reverse map** — needs the font reading machinery; folded into
  **stage 3**.

## Stage 2 — text positioning & metrics — **done (bidi & vertical writing deferred)**

Independent of the Unicode work; fixes layout even with today's partial CMaps.
The landed work covers the local corpus and the bulk of real-world PDFs. The
mechanics live in *Text layout* / *Form XObjects* / *HTML* under *What works*;
this is the summary.

**Architecture decision (2026-06): a renderer-agnostic placed-text emission.**
The content executor produces a per-page list of **placed text items**, each
carrying its text-space → user-space transform (CTM × text matrix, with font
size / horizontal scaling / rise folded in), the resolved font, size, the
text-state spacing parameters, the raw character codes, a Unicode representation,
and per-code advances. The HTML layer consumes that list and decides how to map
it — per-run spans with a CSS `transform` vs. per-glyph positioning. **The core
never commits to either**, pushing the run-vs-glyph question all the way down to
rendering.

**Achieved**
- **Transforms & the placed-text emission.** A 2-D affine `Transform2D`
  (`util/math_util.hpp`); the CTM *concatenates* on `cm`, `Tm`/`Tlm` tracked
  properly (`BT` resets, `Td`/`TD`/`T*` and the line-move half of `'`/`"` advance
  `Tlm` → `Tm`), rise and horizontal scaling folded into the text rendering
  matrix; `extract_text` yields one placed `TextElement` per shown segment, which
  the HTML layer maps to a positioned CSS-`transform` span (page y-axis flipped
  once per page).
- **Glyph advances & metrics.** `/FirstChar`+`/Widths`+`/MissingWidth` (simple)
  and `/W`+`/DW` (composite) parsed into `Font::advance_width`; after each segment
  `Tm` advances by Σ(width × Tfs + Tc [+ Tw for 0x20]) × Th and a `TJ` number
  translates `Tm` by `−n/1000 × Tfs × Th`, so segments, kerning and lines land
  correctly. The element carries the per-code advances so the renderer needn't
  re-derive them.
- **Form XObjects.** `Do` on a `/Form` runs the form content against scoped
  `/Resources` under its `/Matrix`; the `/XObject` table is memoized by reference
  (shared forms parsed once, cyclic references resolve to the existing element)
  and cyclic invocation is guarded at render time. Image XObjects are tagged but
  not decoded (stage 4). Reused by stage 4 (tiling patterns) and stage 5
  (annotation appearances).
- **Render modes & extraction refinements.** The text render mode (`Tr`) rides on
  every element (unpainted 3/7 → transparent-but-selectable span, 1–2/4–6 → a
  painted fill); a segment whose code → Unicode chain yields nothing is marked
  `no_unicode`; an `/ActualText` marked-content sequence overrides the per-glyph
  Unicode of its enclosed shows.
- **Space inference.** A running pen infers the inter-word/-line spaces producers
  omit (in `text` only, codes/advances/placement untouched), so copy/paste and
  search work.

**Deferred**
- **Bidi & vertical writing** — RTL run ordering and vertical writing mode
  (`Identity-V`/CJK, the `/W2`/`/DW2` metrics and a perpendicular pen advance). No
  corpus fixture needs it yet; tracked under *Other known gaps* alongside the
  legacy-CJK CMap work (both wait on a real file).
- **Intra-segment glyph shaping** (browser fallback) and **AFM widths for the
  non-embedded standard-14 fonts** — folded into **stage 3**. `/BBox` clipping,
  `/MCID`-driven structure-tree reordering and `/Alt` (stage 5), and precise
  baseline placement (needs font ascent metrics) are likewise deferred.

## Stage 3 — fonts in HTML

Needed for visual fidelity regardless of text extraction. **Display fidelity is
entirely a stage-3 deliverable**: stages 1–2 produce text + position only, and
today's PoC emits Unicode text in spans with no embedded font
(see *What works*), so even *correctly extracted* text currently renders in a
fallback system font — wrong shapes/metrics, and CJK may show tofu. "Render the
actual glyph" only goes live here; the current fallback-font rendering is
expected, not a bug.

**Decision (2026-06): in-house, no FontForge.** pdf.js proves complete font
conversion is doable without a font library; pdf2htmlEX uses FontForge at the
cost of a notoriously heavy build. No trimmed off-the-shelf alternative does
what we need (FreeType/stb_truetype are read-only; hb-subset can only subset
along the *existing* `cmap`, so it cannot inject the PUA mappings below).
Expected ~5–8k lines of focused C++ — on the order of an `oldms/` module.
Reading (SFNT tables, CFF charsets) is the easy part and also yields the
**embedded-font reverse map**: a font with no usable `/ToUnicode` or `/Encoding`
gets code → Unicode from its embedded program — the TrueType `cmap` reversed, or
Type1/CFF charstring glyph names through the AGL — closing the last gap in the
stage-1 extraction chain.

**Architecture: IR for facts, pass-through for glyphs.** No glyph-level font IR:
decompiling and recompiling outlines is the FontForge model — loses hinting,
risks fidelity, and with one output format (SFNT) the M×N payoff never
materializes. Glyph data (outlines, hinting, charstrings) passes through
byte-for-byte; even Type1 → Type2 charstrings is a direct sibling-format
translation. What *is* shared: a thin `abstract::Font` interface — per-flavor
readers producing the facts every consumer needs (glyph count, glyph → Unicode,
advance widths, units-per-em, name, bbox, symbolic flag) with raw bytes kept
alongside. The embedded-font reverse map (above) reads Unicode from it, the OTF wrap synthesizes
`head`/`hhea`/`hmtx`/`OS/2` from it, the re-encoder assigns PUA code points from
its glyph count.

**Decision (2026-06-19): standalone-first, uniform PUA, Type3 stays in-stage.**
Three sequencing/scope choices fix the sub-stage plan below:
- **Standalone-first.** Fonts ship as a library deliverable — a `FontFile`
  `DecodedFile` plus a specimen-page HTML view, with font-only tests — *before*
  being wired into PDF output. The specimen page's glyph grid must show *every*
  glyph, which forces the PUA re-encode, table-directory rebuild and OTF wrap
  first, against a directly viewable, independently testable artifact.
- **Uniform PUA re-encode.** *Every* font is re-encoded to deterministic Private
  Use Area code points (`U+E000 + glyph index`) for **display** — one pipeline,
  no mapped-vs-unmapped branch (pdf2htmlEX's model), so display is always
  glyph-exact. The extracted Unicode (the stage-1 chain, plus the new
  embedded-font reverse map) is **not** discarded: it rides on the element as
  today and the HTML layer surfaces it for **selection/search** — the
  display/text decoupling holds (see *Design decisions*). Runs with no
  recoverable Unicode are additionally marked non-extractable (`user-select:
  none`, `aria-hidden`).
- **Type3 stays in stage 3.** Type3 glyphs are drawing procedures, so they need
  path → SVG rendering that otherwise belongs to stage 4; a minimal path → SVG
  capability is pulled forward into sub-stage 3.6 rather than waiting on stage 4.

**Sub-stages.** Ordered so each lands an independently testable (and, from 3.2,
viewable) artifact; the risky core — read a font, produce a sanitizer-clean,
fully re-encoded one — is proven by font-only tests before any PDF wiring. Sizing
mirrors stage 2 (one PR each). Each gets its own detailed design before
implementation.

- **3.0 — `abstract::Font` interface + SFNT/TrueType reader (facts only).**
  **Done.** The thin per-flavor reader interface (`abstract::Font`,
  `internal/abstract/font.hpp`) producing the facts every consumer needs (glyph
  count, glyph → Unicode, advance widths, units-per-em, name, bbox, symbolic
  flag), raw glyph bytes kept alongside. First implementation: `SfntFont`
  (`internal/font/sfnt_font.{hpp,cpp}`) — reads the font from a `std::istream`
  (seeking per table, the bytes materialized lazily for the pass-through `data()`),
  parsing the table directory and `head`/`maxp`/`hhea`/`hmtx`/`cmap` (formats
  0/4/6/12)/`name`; `post`/`OS/2` deferred until a consumer needs them. Font-only
  unit tests; no HTML, no PDF wiring.
- **3.1 — OTF wrap + uniform PUA re-encode + table-directory rebuild.** Given an
  `abstract::Font`, emit a browser-loadable, OTS-clean SFNT: rewrite `cmap` to the
  deterministic PUA map over *every* glyph, splice/rebuild the table directory,
  recompute `head.checkSumAdjustment`. The single riskiest piece — exercised by
  font-only round-trip + OTS tests before any UI.
  - **Mechanism (`internal/font/`).** `SfntFont` carries `cmap` as a
    `code point -> glyph` model that is the single source of truth (the
    accessors read it; `write(std::ostream&)` serializes it back into the `cmap`
    table and copies every other table through verbatim). It is read via
    `cmap()` and replaced via `set_cmap()`, which rebuilds the reverse
    (glyph -> lowest code point) lookup so the font stays self-consistent.
    Transforms live *outside* the font in `sfnt_transform` —
    `reencode_to_pua(font)` builds the PUA map and hands it to `set_cmap`.
    Writing is stream-based, mirroring the reader's `std::istream`; the
    whole-file checksum is computed analytically from the additive
    per-table/directory checksums (no second pass, no seekable sink).
  - **LIMITATION — `cmap` serialization is BMP-only.** `serialize_cmap` emits a
    single Windows (3,1) **format-4** subtable: code points must be `<= U+FFFF`,
    and a map containing a beyond-BMP code point (which would need a format-12
    subtable) throws. Within the BMP there is no restriction — the map is split
    into maximal arithmetic runs (`idRangeOffset = 0`, singletons allowed), so
    `glyphIdArray` is never needed. This covers the uniform PUA re-encode (one
    run) and ordinary remaps; format-12 / multi-plane PUA spill-over (and the
    matching >6400-glyph case) is a follow-up.
- **3.2 — `FontFile` as a `DecodedFile` + specimen-page HTML (the standalone
  deliverable).** Precedent: `SvmFile`, `ImageFile`. `FileType` entries + magic
  detection (SFNT `0x00010000`/`OTTO`/`true`/`ttcf`, `wOFF`), a `FontFile`
  `FileCategory`, and `html::translate(FontFile)` emitting a specimen page: a
  name/metrics header plus a glyph grid that shows *every* glyph (incl.
  `cmap`-unreachable ones — exactly what 3.1's re-encode makes addressable), the
  font served via `@font-face`. Scope capped at "specimen page" — no font-editor
  creep. *Optional parallel:* PDF as a container — expose embedded fonts as an
  `abstract::Filesystem` (`/fonts/F1.ttf`, …) reusing the filesystem HTML service
  (as for ZIP/CFB); doubles as the corpus harvester.
- **3.3 — wire TrueType into PDF `@font-face` (first end-to-end PDF win).**
  **Done.** Embedded `FontFile2`/`CIDFontType2` programs are read through
  `abstract::Font`, re-encoded to the PUA (3.1 pipeline) and emitted as
  `@font-face` + glyph spans (a dual layer: visible PUA glyphs + a transparent
  selectable Unicode layer), and their reversed `cmap` recovers Unicode for
  selection — the mechanics live in *Embedded font programs* and *HTML* under
  *What works*. Simple-TrueType glyph selection is best-effort (ISO 32000-1
  9.6.6.4); CFF (`/FontFile3`, 3.4) and Type1 (`/FontFile`, 3.5) leave
  `Font::embedded_font` null and keep the fallback path.
- **3.4 — bare CFF (`FontFile3`/Type1C).** A CFF reader (charset, charstrings,
  private dict) producing an `abstract::Font`; wrap into OTF by synthesizing the ~8
  required SFNT tables (advance widths from `/Widths`/`/W`, not by interpreting
  charstrings). Reuses 3.1 (wrap/PUA), 3.2 (specimen) and 3.3 (PDF wiring)
  unchanged.
- **3.5 — Type1 (`FontFile`).** `eexec` decryption, Type1 → Type2 charstring
  translation, build a CFF, reuse 3.4's CFF → OTF path. Reverse map via charstring
  glyph names → AGL. The hardest single piece, but precisely specified (Adobe T1
  spec; pdf.js as reference).
- **3.6 — Type3 + non-embedded fonts.** Type3 glyph procedures (mini content
  streams, already tokenized by the operator parser) → SVG glyphs via a minimal
  path → SVG capability pulled forward from stage 4. Plus non-embedded fonts:
  substitute the standard 14 + common names with CSS fallbacks and metrics from
  `/Widths` (AFM widths for the standard-14 — closes stage 2's deferred item).

**Mechanisms & guards (ride through 3.1–3.5).**
- **Broken-font long tail.** Real embedded fonts are routinely malformed, and
  browsers run web fonts through a sanitizer (OTS) that silently rejects them.
  Regenerating the table directory (which the wrap/re-encode does anyway) covers
  most of it; start strict, add repair heuristics as real files demand.
- **Test oracles (never shipped).** CI gate: run **OTS** over every produced font
  (test-time only); optionally FreeType as a second oracle. Neither is linked into
  the product.

## Stage 4 — graphics

**Decision (2026-06): SVG generation, no rasterizer.** pdf2htmlEX uses poppler
to render non-text into a per-page background image; we generate SVG instead —
serialization, not rendering. pdf.js proves the full PDF graphics model needs no
native renderer. The PDF and SVG imaging models are close cousins (PostScript
heritage), so the mapping is mostly mechanical. Trade-off: pdf2htmlEX gets the
long tail right for free via poppler, while our fidelity is bounded by operator
coverage — countered by the test oracle below. The rasterized-background
fallback is **rejected**: it reintroduces exactly the renderer dependency this
stage exists to avoid.

- Vector content → inline SVG per page, layered under the text spans: paths,
  fill rules, stroke parameters, transforms; clipping → nested `<clipPath>`;
  tiling patterns → `<pattern>` (form-XObject machinery from stage 2);
  axial/radial shadings (types 2/3) → `linearGradient`/`radialGradient`.
- **Images**: `DCTDecode` → `<img>` JPEG pass-through; Flate/LZW raster → PNG
  encode; inline images (`BI`/`ID`/`EI` — currently not even tokenized correctly
  past `ID`); image masks and SMasks later.
- **SVG residue** — where no 1:1 primitive exists; all at generation time, never
  rasterization: mesh/function shadings (types 1, 4–7) → tessellate into small
  flat polygons (pdf.js's approach); color spaces
  (Separation/DeviceN/Indexed/Lab/ICC) → convert to RGB when emitting (sample
  tint transforms, approximate ICC as sRGB, ignore overprint); transparency:
  `CA`/`ca` → `opacity`, soft masks → `<mask>`, blend modes → `mix-blend-mode`;
  isolated/knockout groups don't map cleanly — punt (rare).
- **Renderer as test oracle, not dependency** (parallels stage 3's OTS gate):
  render corpus fixtures with poppler or pdf.js in CI, screenshot our output,
  perceptual-diff.

## Stage 5 — interaction & navigation

Builds on whatever pages render; needs stage 0 plus destinations from the page
tree, little else.

- **Links**: URI actions and internal `GoTo` destinations (incl. named) as `<a>`
  overlays.
- **Annotation appearances**: render `/AP` appearance streams (form XObjects
  again) for highlights, stamps, form-field appearances; AcroForm
  *interactivity* stays out of scope (read-only).
- **Document outline** (`/Outlines`) → navigation anchors/sidebar.
- **Optional content groups** (layers): honor default visibility; no toggle UI.
- **Metadata** (`/Info`, XMP) into `file_meta()`.
- **Output scaling**: monolithic HTML vs. per-page lazy loading for large
  documents (check what odr's HTML service model already provides first).

## Cross-cutting (any time)

- Route diagnostics through `Logger` instead of stdout/stderr; drop the leftover
  debug code (incl. the `"hi"` marker) in `html/pdf_file.cpp`.
- Grow a corpus: `odr-public` fixtures, the PDF101 "nasty files" collection
  linked in `README.md`; assertion-based tests per stage.
- Spec docs offline under `offline/documentation/PDF/` (ISO 32000-1:2008, ISO
  32000-2:2020, Adobe PDF Reference 1.7, with markdown conversions); still to
  do: fold them into `README.md` in place of the web links.

## Other known gaps

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
- **CMap coverage**: the `ToUnicode` CMap is fully handled (multi-byte codes,
  `bfchar`, both `bfrange` forms, multi-char targets), and a simple font's
  `/Encoding` (base + `/Differences` → AGL) is the fallback when no `ToUnicode`
  stream is present. Composite (Type0) fonts are recognized and extract through
  their `/ToUnicode` or, when absent, a predefined Unicode `/Encoding`
  (`Uni*-UCS2/UTF16/UTF32`). Still open: the legacy CJK code→CID CMaps
  (RKSJ/EUC/Big5/GBK/KSC) and their CID → Unicode tables (large external data; the
  generator scaffolding in `tools/pdf/generate_cid_data.py` is landed, the storage
  decision and lookup remain), and the CFF/Type1 embedded-font reverse maps
  (stages 3.4/3.5 — the TrueType one landed in 3.3); symbolic fonts with a
  built-in encoding default to StandardEncoding until the font program is read
  (now read for TrueType; CFF/Type1 still pending).
- **Bidi & vertical writing** (deferred): RTL run reordering for the
  layout/selection order, and vertical writing mode (`Identity-V`/CJK — the
  `/W2`/`/DW2` vertical metrics and a perpendicular pen advance, which the
  horizontal-only `extract_text` and space inference assume away). No corpus
  fixture needs either yet; revisit when one does.
- **Annotations** are collected but their content is not interpreted (stage 5).
- Revisit the reference-by-lookahead parsing and `read_stream(-1)` fallback.
