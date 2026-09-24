# In-house PDF support (`pdf/`) — design & roadmap

The rules, the non-obvious invariants, and where things live. Reference links
are in [`README.md`](README.md). The copy-order design for the selection layer
is in [`READING_ORDER.md`](READING_ORDER.md).

**Scope.** Parse the file structure (xref tables, xref streams, object streams,
hybrid files, forward-scan recovery), decrypt (RC4, AES-128, AES-256), build
the page tree, execute content streams, and emit HTML: absolutely positioned
text spans placed by the full text transform, vector graphics, images,
shadings, patterns and transparency as inline SVG per page, embedded fonts via
`@font-face`, non-embedded fonts substituted to CSS stacks. The one write path
appends an incremental update (7.5.6) with highlight, underline, strike-out,
squiggly and ink annotations. There is no native renderer and no raster
output.

## Design decisions

- **The browser renders. Display and text are decoupled.** Glyphs render
  through the embedded font, vector content through SVG. Every glyph is
  re-encoded to the Private Use Area (`U+E000 + glyph index`) for display, and
  the extracted Unicode drives selection and search separately. A run with no
  recoverable Unicode is marked non-extractable (`user-select:none`,
  `aria-hidden`). So a text-extraction gap degrades selection, never display.
- **SVG serialization, not rasterization.** A rasterized fallback would bring
  back the renderer dependency the engine exists to avoid. Fidelity is bounded
  by operator coverage, and the reference-output snapshot test guards it.
- **Font handling is in-house.** `abstract::Font` exposes the facts (glyph
  count, glyph to Unicode, advance widths, units per em, name, bbox, symbolic
  flag). Glyph outlines pass through byte for byte. Every embedded flavour
  feeds one path: SFNT (`/FontFile2`), SFNT or bare CFF by magic
  (`/FontFile3`), Type1 to CFF via `type1::to_cff` (`/FontFile`).
- **Code to Unicode is a fallback chain.** In order: `/ToUnicode` CMap; simple
  font `/Encoding` (base plus `/Differences` to glyph name to AGL); composite
  predefined `/Encoding` (`Uni*` CMaps decoded directly, legacy CJK CMaps via
  `pdf_cid` and `pdf_cid_data` as code to CID to Unicode); `/CIDSystemInfo`
  for `Identity-H` and embedded CMaps; the embedded font's reverse map (code to
  glyph to `code_point_for_glyph`). Only a code no link maps yields no
  Unicode, never byte garbage. A simple font's codes are one byte (9.10.3), so
  `to_unicode` imposes that width instead of reading the `/ToUnicode`
  codespace, because producers write `<0000> <FFFF>` there regardless. The
  entries are keyed either way, so a one-byte code is also looked up
  zero-padded. Composite codes split by the codespace, as `Font::codes` does.
- **`std::any` object model.** `Object` holds its value in `std::any` with
  typed `is_*` and `as_*` accessors, like `oldms/`'s `Entry`. The cost: no
  exhaustive matching, and accidental copies are easy. `resolve_object_copy`
  exists because rvalue access does not work (`TODO why rvalue not working?`
  in `pdf_document_parser.cpp`).
- **References are recognized by lookahead, without rewind.** `n g R` reads as
  plain integers until `R` appears, so `read_object` returns the id integer and
  the enclosing context folds it in. `read_array` reacts at the `R` token.
  `read_dictionary` and indirect-object bodies use
  `promote_indirect_reference`: a digit after a value can only be a `gen`,
  because the next token is otherwise a key, `>>` or `endobj`.
- **The element tree is an arena.** `Document` owns every element
  (`vector<unique_ptr<Element>>`). `Catalog`, `Pages`, `Page` and the rest hold
  non-owning pointers plus their original dictionary (`Element::object`), so
  an unmodelled key stays inspectable. Navigation is `is_<T>()` / `as_<T>()`
  over `kids`, backed by `dynamic_cast`.
- **Stream-based parsing, lazy objects.** Everything parses off a
  `std::streambuf`. Random access seeks, sequential tokenizing peeks one char.
  An object parses when referenced and is cached by reference. Positions are
  `std::uint32_t`, so a file of 4 GiB or more is out of scope.
- **Fail early on malformed structure, tolerate unknown content.** A missing
  `obj`, `endobj`, `stream`, `endstream`, `xref` or `startxref`, an unexpected
  char, an unknown page-tree type or an exhausted stream throws
  `std::runtime_error`. A missing or wrong `/Length` is tolerated by scanning
  to `endstream`. An unknown operator is logged and skipped, an annotation
  keeps its raw dictionary, an unmapped CMap code passes through as its
  numeric value, a free or absent reference resolves to null with a warning.
  A throw in the cross-reference layer is caught once, and the file is
  forward-scanned to rebuild a synthetic xref (`recover_xref`; the last
  definition of an id wins). If the trailer has no `/Root`, `recover_root`
  takes the first `/Type /Catalog` it finds in id order.
- **Diagnostics go through `Logger`.** `DocumentParser` and `extract_text`
  take a `const Logger &` (default `Logger::null()`). `Logger` is a value
  handle, so a parser stores it by value.
- **The derived key is never retained.** The empty password is tried first
  (user, then owner), so an owner-locked file opens transparently. After
  `authenticate` the key lives only inside the `Decryptor`. `PdfFile` carries
  the authenticated `Decryptor` from the encryption probe to the render parse.
  Permission bits (`/P`) are recorded, not enforced.

### Baseline placement

PDF's text origin is the baseline. A CSS span anchors its box top, one ascent
above. So each run is raised by one font ascent:

- Uniform branch: the shift comes off `top`
  (`top = (m.f − ascent_em·m.a·size)·pt_to_px`). General branch: the shift
  goes through the matrix, subtracted along the local y axis `(c,d)`. The
  nested PUA glyph layer is positioned relative to its parent, so both share
  one shift.
- `line-height:1` on `.t` and on the nested-glyph placement removes the
  half-leading band, so box top to baseline is the ascent. The re-encode
  synthesizes `OS/2` and `hhea` (`font/cff_transform.cpp`), so our ascent is
  the browser's ascent.
- A non-embedded substitute renders in a local font whose metrics we do not
  control. `SubstituteFontFaces` (`html/pdf_file.cpp`) routes each through a
  generated `@font-face` (`'odr-sN'`, `src: local(...)`) with
  `ascent-override`, `descent-override` and `line-gap-override:0`, deduped by
  (family, style, ascent).
- `ascent_em` (`html/pdf_file.cpp`): FontDescriptor `/Ascent`, else the
  embedded `bbox.y_max / units_per_em`, else `0.8` em, clamped to `[0.5, 1.0]`.
  Open: `/Ascent` wins over the embedded `OS/2` when they disagree, and the
  value is per font, not per CID.

## Non-obvious facts

- `is_decodable()` returns `false` for PDF. Page-tree and content parsing is
  lazy, on the HTML request. `file_meta()` still carries the page count and
  the `/Info` strings, read once at construction, after the empty-password
  unlock. It is all or nothing: a malformed structure leaves `document_type`
  at `unknown`. XMP is not parsed.
- The filter framework hands `DCTDecode` and `JPXDecode` payloads back
  encoded. A JPEG passes through to the browser. A JPEG 2000 goes to `pdf_jpx`
  (openjpeg) and is re-encoded as PNG like every other raster.
  `read_decoded_stream` treats both as an error. The bilevel codecs are in
  house: `pdf_ccitt` covers Group 3 (1-D and 2-D) and Group 4, not the
  uncompressed-mode extension. `pdf_jbig2` covers the arithmetic generic
  regions, symbol dictionaries and text regions a scanner emits. MMR, Huffman,
  refinement and halftone fail the image, not the page. `/JBIG2Globals`
  reaches the filter as a `DecodeOptions`. `Crypt` passes through only as
  `Identity`.
- Inherited page attributes (`Resources`, `MediaBox`, `CropBox`, `Rotate`,
  Table 30) are resolved by an accumulator threaded down the `Pages`
  recursion, not by a `Parent` walk. Lenience, each with a warning: `CropBox`
  defaults to `MediaBox`, `Rotate` is normalized to {0, 90, 180, 270}, a
  missing `MediaBox` is US Letter, missing `Resources` is an empty dict.
- Form XObjects are memoized by reference. A form shared across pages parses
  once, and a cyclic reference resolves to the existing element. An active set
  at render time cuts cyclic invocation (forbidden by 8.10.1, present in real
  files). The form's `/BBox` clips its content.
- Space inference: a pen threaded through the executor prepends one space when
  the gap exceeds 0.2 em along the line or 0.5 em perpendicular. It changes
  `text` only.
- `/ActualText` (marked content, balanced per stream, reset across a form
  invocation) replaces the per-glyph Unicode of the shows it encloses.
- A `no_unicode` run (composite font, no usable mapping) emits no selectable
  span but still renders through the PUA re-encode.
- Type3 fonts paint as ordinary path and image elements (`/CharProcs` run
  through the executor at `/FontMatrix × size × Tm × CTM`) and contribute no
  visible text (`render_as_graphics`, like an invisible `3 Tr` run). The run
  stays selectable. Recursion is cut at depth 8.
- Link annotations resolve in the HTML layer from the raw dictionaries: `/URI`
  to an external `<a>`, `/GoTo` and `/Dest` to `#pN`, named destinations via
  `/Dests` and the `/Names` name tree (depth-guarded). Every `href` goes
  through `html::uri_kind`.
- Annotation appearances paint after the page content (12.5.5). The parser
  resolves `/AP /N` (through `/AS` when it is a state dictionary) into a form
  XObject and the matrix that fits its transformed `/BBox` onto `/Rect`.
  `extract_annotation` runs it like a `Do`, so a form field's value and a
  markup annotation's drawing are ordinary, selectable elements. Hidden and
  NoView (`/F`) and popup annotations paint nothing. The appearance is never
  regenerated from `/V` and `/DA`.
- Text clips (`Tr` 4 to 7, 9.3.6) collect as `TextClipRun`s until `ET` makes
  them one `ClipPath`. The HTML writer draws it as `<text>` in the glyph
  layer's `@font-face`. Every glyph has its own `x`, so SVG white-space
  handling moves nothing.
- CMYK is naive (no ICC) and overprint is ignored. CIE, ICCBased, Indexed,
  Separation, DeviceN and Lab resolve to RGB at emission by sampling the tint
  `/Function` (types 0, 2, 3, 4).

## Module layout

| File (`pdf/`) | Role |
|---|---|
| `pdf_object.*` | `Object` (`std::any`), `Array`, `Dictionary`, `Name`, strings, `ObjectReference`; dumping |
| `pdf_object_parser.*` | Tokenizer over `std::streambuf` |
| `pdf_file_object.*` | File-structure entries: `Header`, `IndirectObject`, `Trailer`, `Xref`, `StartXref`, `Eof`; `ObjectStream`; `Xref::merge_hybrid` |
| `pdf_file_parser.*` | File-level reads: indirect objects, `read_xref`, `read_xref_stream_table`, trailer, `seek_start_xref`, stream payloads, recovery scan |
| `pdf_filter.*` | `/Filter` and `/DecodeParms` chain: ASCIIHex, ASCII85, LZW, Flate, RunLength, TIFF and PNG predictors; image codecs returned undecoded |
| `pdf_ccitt.*`, `pdf_jbig2.*`, `pdf_jpx.*` | Bilevel and JPEG 2000 decoders |
| `pdf_image.*` | Image bytes to browser-ready JPEG or PNG through the colour space |
| `pdf_document_parser.*` | `parse_document()`: xref chain, catalog, page tree; lazy cached objects; `/XObject` table with the dedup and cycle cache; embedded font programs and `/CIDToGIDMap`; annotation appearances |
| `pdf_encryption.*` | Standard security handler: `Authenticator` to `Decryptor` (RC4, AES-128, AES-256); `standard_security` holds the pure algorithms for known-answer tests |
| `pdf_document.*` | `Document`: the element arena and `catalog`; `Font::to_unicode`, `Font::glyph_for_code`, `Font::advance_width` |
| `pdf_document_element.hpp` | `Catalog`, `Pages`, `Page`, `Annotation`, `Resources`, `XObject`, `Font` (Type0 facts, glyph metrics, `embedded_font`) |
| `pdf_cmap*.*` | `CMap` and the `ToUnicode` CMap stream parser |
| `pdf_encoding.*` | Simple-font `/Encoding` to Unicode: base tables, `/Differences`, AGL |
| `pdf_cid.*` | Composite predefined `/Encoding` to Unicode; `cid_to_unicode(registry, ordering, cid)` |
| `pdf_cid_data.*` | Generated by `tools/pdf/generate_cid_data.py`: packed legacy-CMap and collection tables |
| `pdf_encoding_data.*` | Generated by `tools/pdf/generate_encoding_data.py`: base encodings and AGL |
| `pdf_afm.*`, `pdf_afm_data.*` | The standard 14 fonts' AFM metrics and the substitute resolution for a non-embedded font |
| `pdf_color.*`, `pdf_function.*`, `pdf_shading.*` | Colour spaces to sRGB, PDF functions (types 0, 2, 3, 4), axial and radial shadings pre-sampled to stops |
| `pdf_graphics_operator*.*` | Operator enum, `GraphicsOperator`, content-stream tokenizer |
| `pdf_graphics_state.*` | `GraphicsState`: state stack, `execute(op)`, CTM, `Tm`, `Tlm`, `text_placement_transform()`, `advance_text()`, `TextClipRun` |
| `pdf_page_element.hpp`, `pdf_page_extractor.*` | `extract_text`: content to placed text, path, image, shading and pattern elements; `Do` recursion; marked content; space inference |
| `pdf_file.*` | `abstract::PdfFile`: probes encryption at construction, carries the `Decryptor` forward; `annotate` parses the json wire format |
| `pdf_writer.*` | `IncrementalWriter`: copies the source through and appends the changed objects under their own xref in the file's flavour. Refuses a recovered or encrypted file |
| `pdf_annotation.*` | The markup and ink annotations and their appearance streams. Only the highlight blends Multiply (11.6.4.1) |
| `util/math_util.hpp` | `util::math::Transform2D`, PDF row-vector convention |

Consumers outside the module: `open_strategy.cpp` and `html/pdf_file.cpp`
(`create_pdf_service`; the per-font PUA re-encode, OTF wrap and `@font-face`;
the dual-layer glyph and Unicode spans; uses `font/` `sfnt_*`, `cff_*`,
`type1_*`).

The reference-output snapshot test (`test/data/reference-output/`) is the
graphics oracle. Each change regenerates it, and the diff is reviewed.

## Tests

`test/src/internal/pdf/` is assertion-based: inline strings, mini-PDFs from
`pdf_test_file_builder`, and a few end-to-end fixtures. One file per module
file: filters, file objects, encryption (R 2, 3, 4, 6, with vectors from
fixtures and from frozen `qpdf --encrypt` output), the document parser
(inherited attributes, recovery, composite fonts, form-XObject cycles,
end-to-end decryption), the code-to-Unicode chain per link, the page extractor
(placement, `TJ`, render modes, `ActualText`, space inference), fonts, colour,
functions, shadings, images, the three codecs, the writer and the annotations.
`test/src/internal/crypto/crypto_util_test.cpp` and
`test/src/internal/util/math_util_test.cpp` cover the primitives. A private
fixture is skipped when the test data is not fetched.

Above the module: `test/src/pdf_annotate_test.cpp` (the `annotate` API),
`test/src/html_test.cpp` (text clips), and `test/browser/annotation/` for the
browser side. The span emission and CSS transform mapping have no assertion
test. The snapshot test guards them.

# Roadmap

Each item gets its own design before implementation. Grow the corpus with it
(odr-public fixtures plus the PDF101 collection in `README.md`).

## Interaction and navigation

- Remote and launch actions (`/GoToR`, `/Launch`). Destination position and
  zoom: the internal-link handler uses only the target page.
- Document outline (`/Outlines`).
- Optional content groups: honour default visibility, no toggle UI.
- Output scaling: one HTML file versus per-page lazy loading. Check what the
  HTML service already provides first.
- Copy order of the selection layer: [`READING_ORDER.md`](READING_ORDER.md).

## Known gaps

- Graphics:
  - Soft masks and clips are not applied to text. The glyphs live in the HTML
    text layer, not the SVG. A text clip whose font has no `@font-face`
    (Type3, not embedded) is left out. Isolated and knockout groups (`/I`,
    `/K`) are not distinguished. A soft mask's group renders with a black
    backdrop unless `/BC` is a plain device colour.
  - Mesh and function-based shadings (types 1, 4 to 7) are not emitted.
  - `/Extend`, `/Background` and `/BBox` on a shading are parsed but not
    honoured. SVG `pad` spread always applies, so a non-extended shading
    over-paints past its interval.
  - An overlapping tiling lattice (`/PatternType 1`, step smaller than
    `/BBox`) has no single `<pattern>` equivalent and is lost. Nested content
    inside a tile is skipped.
  - No perceptual-diff oracle. Only the snapshot test gates graphics.
- Encryption: per-stream `/Crypt` `Name` overrides and `Perms` (Algorithm 13)
  validation. Public-key handler and revision 5 are out of scope.
- Recovery: the constructor-triggered recovery cannot decode object streams in
  an encrypted broken file, because there is no decryptor yet.
- Linearized files get no special handling. Hint streams are ignored.
- Bidi and vertical writing: RTL reordering and vertical mode (`Identity-V`,
  the `/W2` and `/DW2` metrics, perpendicular pen advance). `extract_text` and
  space inference assume horizontal text.
- XMP metadata (`/Metadata`) is not parsed.
