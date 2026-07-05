# In-house PDF support (`pdf/`) — design & roadmap

The **why** behind the `pdf/` module and the roadmap. What is concretely
implemented is in the code; this file keeps the rationale, the non-obvious
invariants, and where things live. Reference links live in [`README.md`](README.md).

This is the `DecoderEngine::odr` path for PDF. The sibling `../pdf_poppler/`
module (poppler / pdf2htmlEX, behind `ODR_WITH_PDF2HTMLEX`) is the
production-quality alternative engine; this one is experimental.

**Goal.** Faithful read-only HTML for common real-world PDFs through a
pure-serialization pipeline (no native renderer), so the poppler engine becomes
optional rather than required. The file-format, text-extraction, font and
graphics foundations are in place; what remains is interaction & navigation plus
a tail of known gaps (see *Roadmap*).

**Scope in one line.** Parse the PDF object/file structure (xref tables, xref
streams, object streams, hybrid files, forward-scan recovery), decrypt (RC4,
AES-128, AES-256), build the page tree, tokenize content streams, and emit HTML:
absolutely-positioned text spans placed by the full text transform, with vector
graphics / images / shadings / patterns / transparency as inline SVG per page,
embedded fonts via `@font-face` and non-embedded fonts substituted to CSS stacks.

---

## Design decisions (the load-bearing part)

**Rendering is deferred to the browser; display and text are decoupled.** We emit
no rasterized output — glyphs render via the embedded font (`@font-face`), vector
content via SVG. Because display is driven by the *glyph*, not the extracted
Unicode, **text-extraction gaps degrade only selectability/search — never
display.** Every glyph is re-encoded to the Private Use Area (`U+E000 + glyph
index`, the uniform re-encode, decision 2026-06-19) for display, with the
extracted Unicode carried separately to drive selection/search; runs with no
recoverable Unicode are additionally marked non-extractable
(`user-select:none`, `aria-hidden`). So even an unshipped-CMap CJK PDF *looks*
right; only its selectability is affected.

**No native renderer — SVG serialization, not rasterization (decision, 2026-06).**
pdf.js proves the full PDF graphics model needs none, and the PDF and SVG imaging
models are close cousins, so the mapping is mostly mechanical. The rasterized-
background fallback is rejected: it reintroduces the exact renderer dependency the
`odr` engine exists to avoid. Fidelity is bounded by operator coverage, not a
renderer's long tail; the reference-output snapshot test guards it.

**Font handling is in-house, facts-vs-glyphs split.** No FontForge (pdf.js proves
it unnecessary; heavy build, and no read-only library can inject the PUA mappings
we need). A thin `abstract::Font` exposes the *facts* every consumer needs (glyph
count, glyph→Unicode, advance widths, units-per-em, name, bbox, symbolic flag)
while glyph outlines pass through **byte-for-byte** (no decompile/recompile).
Every embedded flavor feeds the one path: SFNT (`/FontFile2`), bare CFF
(`/FontFile3`), Type1 (`/FontFile` → CFF via `type1::to_cff`).

**Text mapping is a fallback chain, not one method.** code → Unicode is tried in
order: `/ToUnicode` CMap → simple-font `/Encoding` (base + `/Differences` → glyph
name → AGL) → composite predefined `/Encoding` (`Uni*` Unicode CMaps decoded
directly; legacy CJK CMaps via `pdf_cid`/`pdf_cid_data`: code → CID → Unicode) →
`/CIDSystemInfo` collection for `Identity-H`/embedded-CMap → embedded-font reverse
map (code → glyph → `code_point_for_glyph`). Only a genuinely unmapped code yields
"no Unicode" — never byte-garbage. The point of the chain is that each link
recovers a class of real-world PDF the previous one misses.

**`std::any`-based object model.** `Object` holds its value in `std::any` with
typed `is_*`/`as_*` accessors (mirrors `oldms/`'s `Entry`). Pro: one value type
throughout parser, document elements, operator arguments. Con: no exhaustive
matching, RTTI lookups, accidental copies easy — `resolve_object_copy` exists
because rvalue access proved fiddly (see the `TODO why rvalue not working?` in
`pdf_document_parser.cpp`).

**References recognized by lookahead, rewind-free.** `n g R` reads as plain
integers until `R` appears, so standalone `read_object` returns the *id* integer
and the enclosing context folds it in. `read_array` reacts at the `R` token
(array elements may be bare adjacent integers, so it can't promote earlier);
`read_dictionary`/indirect-object bodies use `promote_indirect_reference` (a digit
after a value can only be a `gen`, since the next token is otherwise a key, `>>`,
or `endobj`).

**Element tree as an arena.** `Document` owns all elements
(`vector<unique_ptr<Element>>`); `Catalog`/`Pages`/`Page`/… hold raw non-owning
pointers plus their original dictionary (`Element::object`), so unmodelled keys
stay inspectable. Navigation is typed `is_<T>()`/`as_<T>()` `dynamic_cast`
wrappers over `kids` (the former `Type` tag enum was dropped for RTTI).

**Stream-based parsing, lazy objects.** Everything parses off a
`std::istream`/`std::streambuf` — no full-file buffer. Random access seeks;
sequential tokenizing uses single-char peek/bump. Objects parse only when
referenced and cache by reference (shared objects read once). Positions are
`std::uint32_t` — **files ≥ 4 GiB are out of scope.**

**Fail early on malformed structure, tolerate unknown content.** Structural
surprises **throw** `std::runtime_error` (missing `obj`/`endobj`/`stream`/
`endstream`/`xref`/`startxref`, unexpected chars, unknown page-tree element type,
stream exhaustion). A missing/unresolvable `/Length` is tolerated (scan to
`endstream`). Unknown *content* is tolerated: unrecognized operators logged and
skipped, annotations keep their raw dictionary, unmapped CMap codes pass through
as their numeric value, free/absent object refs resolve to null with a warning.
**Crucially, a structural throw in the cross-reference layer is not fatal** — it
is caught once and the file is forward-scanned to rebuild a synthetic xref
(handles e.g. an HTTP response saved as `.pdf`, every offset shifted by the
header) before giving up.

**Diagnostics route through `Logger`.** `DocumentParser` and `extract_text` take a
`Logger &` (default `Logger::null()`); no stray `stdout`/`stderr` remains — new
diagnostics follow the same pattern.

**Encryption: the derived key is never retained.** The empty password is tried
first (user then owner), so owner-locked files open transparently. Once
`authenticate` succeeds the key lives only inside the `Decryptor` (no accessor);
`PdfFile` carries the authenticated `Decryptor` forward from the encryption probe
to the render parse, so the HTML service unlocks without re-deriving. Permission
bits (`/P`) are recorded, not enforced.

### Baseline placement (why the CSS is the way it is)

PDF's text origin is the glyph *baseline*; a CSS span anchors its box *top*, one
ascent above. Left uncorrected, every run renders ~one ascent too low
(highlights/underlines, painted correctly from user space, then float a line
above their text). So each run is raised by one font ascent:

- **Uniform branch**: shift comes off `top`
  (`top = (m.f − ascent_em·m.a·size)·pt_to_px`). **General branch**: it goes
  *through* the matrix, subtracted along the local y axis `(c,d)` (it can't be
  applied to `m.f` after the fact). The dual layer shares one shift (the nested
  PUA glyph layer is positioned relative to its parent).
- **`line-height:1`** (on `.t` and the nested-glyph placement) zeroes the
  half-leading band so box-top→baseline is just the ascent; default `normal`
  would add an unknown offset. Exact when ascent+descent ≈ 1 em. We control our
  rendering metric: the re-encode synthesizes `OS/2`/`hhea`
  (`font/cff_transform.cpp`), so our ascent matches the browser's.
- **Non-embedded substitutes** render in a *local* font whose metrics we don't
  control. `SubstituteFontFaces` (`pdf_file.cpp`) routes each through a generated
  `@font-face` (`'odr-sN'`, `src: local(...)`) carrying `ascent-override` /
  `descent-override` / `line-gap-override:0`, so the browser positions the
  baseline from *our* metric. Deduped by (family, style, ascent).
- **`ascent_em`** (`pdf_file.cpp`): FontDescriptor `/Ascent`, else embedded
  `bbox.y_max / units_per_em`, else `0.8` em; clamped `[0.5, 1.2]`. Font-format
  agnostic (only `abstract::Font` virtuals + the descriptor). Open: `/Ascent` vs
  embedded `OS/2` disagreement (currently `/Ascent` wins); per-font not per-CID;
  no focused unit test yet (guarded by the reference snapshot).

---

## Non-obvious facts worth knowing

Things the code won't shout at you:

- **`is_decodable()` returns `false`** for PDF; page-tree/content parsing is lazy
  (on HTML request). But `file_meta()` still carries a best-effort `document_meta`
  (page count + `/Info` strings), read once at construction (an owner-locked file
  is unlocked with the empty password first so `/Info` decrypts). XMP is not
  parsed — `document_meta` is `/Info`-only.
- **Image codecs are deliberately not decoded** in the filter framework
  (DCTDecode/JPXDecode/CCITTFaxDecode/JBIG2Decode): `decode()` stops and hands
  back the still-encoded payload for the image path; `read_decoded_stream` treats
  them as an error. `Crypt` passes through only as `Identity`.
- **Inherited page attributes** (`Resources`/`MediaBox`/`CropBox`/`Rotate`, Table
  30) are resolved by threading an accumulator down the `Pages` recursion — *not*
  by a `Parent` walk. Lenience (all with a `Logger` warning): `CropBox` ←
  `MediaBox`, `Rotate` normalized to {0,90,180,270}, missing `MediaBox` → US
  Letter, missing `Resources` → empty dict.
- **Form XObjects are memoized by reference**, so a form shared across pages is
  parsed once and a cyclic form reference resolves to the existing element (the
  in-memory graph mirrors the file, cycles included). A render-time active-set
  guard cuts cyclic *invocation* (spec forbids it, 8.10.1, but real files do it).
  `/BBox` clipping is deferred.
- **Space inference**: producers omit inter-word/-line spaces. A running pen
  threaded through the executor prepends a single space when the gap exceeds
  ~0.2 em along the line or ~0.5 em perpendicular — in `text` only
  (codes/advances/placement untouched).
- **`/ActualText`** (marked content, balanced per stream, reset across form
  invocation) overrides the per-glyph Unicode of its enclosed shows: emitted once,
  rest of the sequence suppressed.
- **`no_unicode` runs** (composite font, no `/ToUnicode` or usable encoding) emit
  no selectable span but still render via the embedded program's PUA re-encode.
- **Type3 fonts** paint as ordinary path/image elements (`/CharProcs` run through
  the graphics executor at `/FontMatrix × size × Tm × CTM`) but contribute no
  visible text of their own (`render_as_graphics`, like an invisible `Tr 3` run);
  the run stays selectable. Depth-guarded.
- **Link annotations** resolve entirely in the HTML layer from raw annotation
  dictionaries (no parser/IR change): `/URI` → external `<a>`, `/GoTo`/`/Dest` →
  internal `#pN` (each page `div` carries `id="pN"`), named dests via `/Dests` +
  the `/Names` name tree (depth-guarded).
- **CMYK is naive (no ICC); overprint ignored.** CIE/ICCBased/Indexed/Separation/
  DeviceN/Lab resolve to RGB at emission by sampling the tint `/Function` (types
  0/2/3/4).

---

## Module layout

| File (`pdf/`) | Role |
|---|---|
| `pdf_object.{hpp,cpp}` | Object model: `Object` (`std::any` variant), `Array`, `Dictionary`, `Name`, strings, `ObjectReference`; dumping |
| `pdf_object_parser.{hpp,cpp}` | Tokenizer over `std::streambuf` |
| `pdf_file_object.{hpp,cpp}` | File-structure entries (`Header`/`IndirectObject`/`Trailer`/`Xref` tagged-union/`StartXref`/`Eof`); `parse_xref_stream_table`, `ObjectStream` |
| `pdf_file_parser.{hpp,cpp}` | File-level reads on `ObjectParser`: indirect objects, xref, trailer, startxref, stream payloads, `seek_start_xref` |
| `pdf_filter.{hpp,cpp}` | `/Filter`/`/DecodeParms` chain: ASCIIHex/ASCII85/LZW/Flate/RunLength + TIFF/PNG predictors; image codecs returned undecoded |
| `pdf_document_parser.{hpp,cpp}` | `parse_document()`: xref/trailer chain → catalog → page tree; lazy cached object reads; resources incl. `/XObject` table with the dedup/cycle-breaking cache; loads embedded font programs + `/CIDToGIDMap` |
| `pdf_encryption.{hpp,cpp}` | Standard security handler: `Authenticator` → `Decryptor` (RC4, AES-128/256) + a `standard_security` namespace of pure algorithms for known-answer tests |
| `pdf_document.hpp` | `Document`: arena of `Element`s + `catalog` pointer |
| `pdf_document_element.hpp` | Element structs: `Catalog`/`Pages`/`Page`/`Annotation`/`Resources`/`XObject`/`Font` (incl. Type0 facts, glyph metrics, `embedded_font`/`glyph_for_code`, `to_unicode` fallback) |
| `pdf_cmap*.{hpp,cpp}` | `CMap` + the `ToUnicode` CMap stream parser |
| `pdf_encoding.{hpp,cpp}` | Simple-font `/Encoding` → Unicode: base tables, `/Differences`, AGL |
| `pdf_cid.{hpp,cpp}` | Composite predefined `/Encoding` → Unicode (Unicode CMaps direct; legacy CJK code→CID→Unicode); `cid_to_unicode(registry, ordering, cid)` |
| `pdf_cid_data.{hpp,cpp}` | **Generated** (`tools/pdf/generate_cid_data.py`): S3-packed legacy-CMap + collection tables. ~0.58 MB compiled |
| `pdf_encoding_data.{hpp,cpp}` | **Generated** (`tools/pdf/generate_encoding_data.py`): base encodings + AGL |
| `util/math_util.hpp` | `util::math::Transform2D`: 2-D affine (PDF row-vector convention) |
| `pdf_graphics_operator*.{hpp,cpp}` | Operator enum + `GraphicsOperator`; content-stream tokenizer |
| `pdf_graphics_state.{hpp,cpp}` | `GraphicsState`: state stack, `execute(op)` for the modelled subset; CTM/`Tm`/`Tlm`; `text_placement_matrix()`, `advance_text()`; `save`/`restore`/`concat_matrix` reused by `q`/`Q`/`cm` and form invocation |
| `pdf_page_text.{hpp,cpp}` | `extract_text`: content → `TextElement` per shown segment; `Do` recursion; marked-content/`ActualText`; pen-based space inference |
| `pdf_file.{hpp,cpp}` | `abstract::PdfFile`; probes encryption at construction, carries the authenticated `Decryptor` forward |

Consumers outside the module: `open_strategy.cpp` (detection/engine selection) and
`html/pdf_file.cpp` (`create_pdf_service`; the per-font PUA re-encode + OTF wrap +
`@font-face`, and the dual-layer glyph/Unicode span emission, using `font/` —
`sfnt_*`, `cff_*`, `type1_*`).

The **reference-output snapshot test** (`test/data/reference-output/`) is the
graphics oracle: each change regenerates it and the diff is reviewed. It is now
the **odr engine's own output only** — the `*.pdf-poppler` / `*.doc-wvware`
cross-engine snapshots were removed once the odr engine could replace them.

---

## Tests

`test/src/internal/pdf/` — all **assertion-based**, inline strings or
test-builder mini-PDFs; a handful of end-to-end fixtures. What is covered:

- **`pdf_filter`** — every decoder, predictors, chains, image-codec stop,
  `Crypt`/unknown-filter errors.
- **`pdf_file_object`** — xref-stream entry decoding, `ObjectStream`,
  `Xref::append`/`merge_hybrid` precedence.
- **`pdf_encryption`** + `crypto_util_test` — the standard handler across R 2/3/4/6
  (vectors from real fixtures and frozen `qpdf --encrypt` output — no fixture
  ships, no test is circular); MD5/RC4/SHA-384/512 primitives vs public vectors.
- **`pdf_document_parser`** — whole-file mini-PDFs via `pdf_test_file_builder`
  (classic + xref-stream), inherited-page-attributes, cross-reference recovery,
  composite fonts, glyph metrics, form-XObject cycles/sharing. End-to-end:
  `style-various-1.pdf`, RC4 + AES-256 decryption, HTTP-saved-as-PDF recovery
  (private fixtures skipped when the submodule is absent).
- **`pdf_file_parser`** — sequential walk + xref/trailer/root navigation.
- **`pdf_cmap` / `pdf_encoding` / `pdf_cid`** — the code→Unicode chain per link.
- **`pdf_page_text`** — `extract_text` placement, glyph advances, `TJ`, form
  XObjects, render modes, `ActualText`, space inference.
- **`pdf_font_program`** — `glyph_for_code` + embedded reverse-map `to_unicode`.
- **`math_util_test`** — `Transform2D`.

**Not yet asserted:** the HTML output itself (span emission / CSS transform
mapping, dual-layer glyph/Unicode). Several `odr-private` xref/objstm/hybrid
fixtures are verified manually but not pinned.

---

# Roadmap

The next feature cluster is **interaction & navigation**; the rest is a tail of
known gaps. Each remaining item gets its own detailed design before
implementation. Grow the corpus alongside (odr-public fixtures + the PDF101
"nasty files" collection linked in `README.md`; assertion tests per feature).

## Interaction & navigation

Link annotations (`/URI` + internal `/GoTo`) already land. Remaining:

- **Annotation appearances**: render `/AP` streams (form XObjects again) for
  highlights/stamps/form-field appearances; AcroForm *interactivity* stays out of
  scope (read-only).
- **Remote/launch actions** (`/GoToR`, `/Launch`) and destination scroll
  position/zoom (the internal-link handler uses only the target page).
- **Link overlays vs. text selection**: `<a>` overlays sit above `.sel` text, so
  text under a link can't be selected. A JS workaround (overlays
  `pointer-events:none`, a click handler re-hit-tests via `elementFromPoint`) was
  built then **reverted as too involved** (commit `5cfa8a09`, reachable for
  later). Options: (a) reinstate it; (b) a CSS-only route if one exists;
  (c) accept clickable-only.
- **Document outline** (`/Outlines`) → nav anchors/sidebar.
- **Optional content groups** (layers): honor default visibility, no toggle UI.
- **Output scaling**: monolithic HTML vs. per-page lazy loading (check what odr's
  HTML service model already provides first).

## Known gaps

- **Graphics deferrals**:
  - **Soft masks on *text*** are not applied (the visible glyphs live in the HTML
    text layer, not the SVG; graphic content *is* masked). Isolated/knockout group
    semantics (`/I`, `/K`) not distinguished; a soft mask's own group renders with
    its default (black) backdrop unless `/BC` is a plain device colour.
  - **Mesh & function-based shadings** (types 1, 4–7): only axial (2) / radial (3)
    are emitted; the rest would tessellate into flat polygons — not done.
  - **`/Extend`/`/Background`/`/BBox`** on shadings parsed but not honoured (always
    SVG `pad` spread), so a non-extended shading over-paints past its interval.
  - **Overlapping tiling lattices** (`/PatternType 1` step < `/BBox`) can't be one
    SVG `<pattern>` and are not reproduced; nested content inside a tile skipped.
  - **Text clipping** (`Tr` 4–7 / clip paths on text) not applied.
  - **Perceptual-diff oracle**: only the odr-output snapshot test gates graphics;
    an automated pdf.js screenshot-diff is not built.
- **Encryption edge cases**: per-stream `/Crypt` `Name` overrides, the
  `EncryptMetadata false` metadata-stream `Identity` special case, `Perms`
  (Algorithm 13) validation. Public-key handler and revision 5 out of scope.
- **Recovery limits**: with several surviving `/Type /Catalog` objects the first
  in id order wins (not the newest); the constructor-triggered recovery path
  can't decode object streams in an *encrypted* broken file (no decryptor yet).
- **Linearized files** not handled specially (tail-first read usually still works;
  hint streams ignored).
- **Bidi & vertical writing**: RTL reordering and vertical mode (`Identity-V`/CJK,
  the `/W2`/`/DW2` metrics + perpendicular pen advance) deferred — `extract_text`
  and space inference assume horizontal. No corpus fixture needs either yet.
- **XMP metadata** (`/Metadata` packet) not parsed; would supplement/override
  `/Info`.
- **Spec docs offline** under `offline/documentation/PDF/` (ISO 32000-1/-2, Adobe
  1.7) planned, to replace the web links in `README.md`.
