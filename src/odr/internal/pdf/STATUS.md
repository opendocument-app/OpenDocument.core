# In-house PDF support (`pdf/`) — status & design notes

Documents what the `pdf/` module does **today** and the **design decisions**
behind it. Open work is collected [below](#out-of-scope--open-work); the
staged roadmap lives in [`PLAN.md`](PLAN.md). Reference links live in
[`README.md`](README.md) (web resources for now; offline spec docs are
planned). The in-house module is the `DecoderEngine::odr` path for PDF; the
sibling `../pdf_poppler/` module (poppler / pdf2htmlEX, behind
`ODR_WITH_PDF2HTMLEX`) is the production-quality alternative engine.

**Scope.** Parse the PDF object/file structure (classic cross-reference
tables, cross-reference streams, object streams, hybrid files), build the page
tree with fonts and annotations, tokenize page content streams into graphics
operators, and emit a **proof-of-concept HTML rendering**: absolutely
positioned text spans per `Tj` operator, pages sized from `MediaBox`. No
graphics, no images, no font files (encrypted files are decrypted — RC4,
AES-128, AES-256; see [Out of scope](#out-of-scope--open-work) for the
remaining edge cases). The module is experimental and not
wired up to production quality — the HTML path still contains debug
`std::cout` output.

---

## What works

- `.pdf` is detected by file magic and opened as `PdfFile`
  (`DecoderEngine::odr`); `is_decodable()` returns `false` and
  `file_meta()` carries only the file type — no document metadata is read at
  open time. All parsing happens lazily when HTML is requested.
- **Object syntax**: null, booleans, integers/reals, names (incl. `#xx`
  escapes), literal strings (incl. `\` escapes and `\ooo` octal), hex strings,
  arrays, dictionaries, and indirect references (`n g R`) — both standalone
  and nested in arrays/dictionaries.
- **File structure**: header, `n g obj … endobj` indirect objects, `stream`
  payloads (via `/Length`, with a scan-to-`endstream` fallback), classic
  `xref` tables, `trailer`, `startxref`, `%%EOF`; both sequential reading
  (`read_entry`) and random access via the xref table.
- **Incremental updates**: `parse_document` finds `startxref` by scanning the
  file tail, then follows the trailer `Prev` chain (with a cycle guard),
  merging xref tables so the newest entry for each object wins.
- **Cross-reference streams, object streams, hybrid files** (PDF 1.5+,
  stage 0 WP2): each section of the trailer chain may be a classic table or a
  cross-reference stream (`/W`/`/Index`/`Size`, decoded via the filter
  framework, entry types 0/1/2; unknown types treated as absent). Xref
  entries are a tagged union (`FreeEntry`/`UsedEntry`/`CompressedEntry`);
  compressed objects are read from their object stream (`/N`/`/First` header,
  decoded once and cached per stream). Hybrid files follow the
  `XRefStm`-before-`Prev` lookup order, so hidden objects are found. Lenient
  where the wild demands it: `/Type /XRef` only warns, references to free or
  absent objects resolve to null with a `Logger` warning instead of throwing,
  and `n g obj` need not end with a newline.
- **Page tree**: `Catalog` → `Pages` (recursive) → `Page` with per-page
  `Resources` (fonts only) and `Annots` (raw dictionary only). Parsed objects
  are cached by reference (`DocumentParser::m_objects`).
- **Inherited page attributes** (stage 0 WP3): the inheritable set per spec
  Table 30 — `Resources`, `MediaBox`, `CropBox`, `Rotate` — is resolved by
  threading an accumulator down the `Pages` recursion (no `Parent` walk).
  Each `Page` carries the resolved `media_box`/`crop_box`/`rotate` and its
  resolved `resources`. Lenience: `CropBox` defaults to `MediaBox`, `Rotate`
  is normalized to {0,90,180,270}, a `MediaBox` missing everywhere falls back
  to US Letter and a missing `Resources` to an empty dict — both with a
  `Logger` warning rather than throwing.
- **Stream filters** (`pdf_filter`, stage 0 WP1): `/Filter` and
  `/DecodeParms` are honoured, including chains and the inline-image
  abbreviations — FlateDecode and LZWDecode (both with TIFF and PNG
  predictors), ASCIIHexDecode, ASCII85Decode, RunLengthDecode. Image codecs
  (DCTDecode, JPXDecode, CCITTFaxDecode, JBIG2Decode) are deliberately not
  decoded: `decode()` stops and hands back the still-encoded payload for
  stage 4; `read_decoded_stream` treats them as an error. The `Crypt` filter
  passes through only as `Identity`.
- **Encryption** (`pdf_encryption`, stage 0 WP4): the standard security
  handler. A `Decryptor` parses `/Encrypt`, authenticates the password (user
  then owner; the empty password is tried first, so owner-locked files open
  transparently) and decrypts object strings and streams. RC4 (V 1/2, R 2/3,
  40-128 bit), AES-128 crypt filters (V 4, R 4 — `StdCF` with `V2`/`AESV2`,
  `Identity`, honouring `StmF`/`StrF`) and AES-256 (V 5, R 6, AESV3) are all
  supported, including owner-only files and `EncryptMetadata false`. Streams
  are decrypted before `/Filter` decoding; cross-reference streams and
  object-stream members are left untouched (never / already decrypted).
  `PdfFile` answers `password_encrypted()` / `decrypt()` honestly. The user
  password is never retained: once `authenticate` succeeds, the derived key
  lives only inside the `Decryptor` (no accessor), and `PdfFile` carries the
  whole authenticated `Decryptor` forward — from the encryption probe to the
  render parse — so the HTML service unlocks the document without the password
  and without re-deriving the key. Permission bits (`/P`) are recorded, not
  enforced.
- **Fonts / text mapping**: a font's `ToUnicode` CMap stream is decoded via
  the filter framework and parsed; `bfchar` mappings with 1-byte glyph codes
  and single UTF-16 units are applied. Unmapped glyphs pass through as their
  byte value.
- **Content streams**: the full graphics-operator vocabulary (per the cheat
  sheet linked in `README.md`) is tokenized; `GraphicsState` executes a subset
  (state save/restore stack `q`/`Q`, matrices `cm`/`Tm`, line parameters, text
  state `Tc`/`Tw`/`Tz`/`TL`/`Tf`/`Tr`/`Ts`, text positioning `Td`/`TD`,
  grey/RGB/CMYK colors, glyph metrics `d0`/`d1`). Unknown operators are logged
  to stderr and skipped.
- **HTML**: one `document.html` view; each page is a `div` sized from
  `MediaBox` (points → inches), each `Tj` becomes an absolutely positioned
  `span` at the text-state offset with `font-size` from `Tf` and the
  CMap-translated text. `TJ`/`'`/`"` are recognized but only printed to
  stdout, not rendered.

## Module layout

| File (`pdf/`)                          | Role                                                  |
|----------------------------------------|-------------------------------------------------------|
| `pdf_object.{hpp,cpp}`                 | Object model: `Object` (`std::any`-based variant), `Array`, `Dictionary`, `Name`, `StandardString`/`HexString`, `ObjectReference`; `to_stream`/`to_string` dumping |
| `pdf_object_parser.{hpp,cpp}`          | Tokenizer over `std::streambuf`: whitespace/lines, numbers, names, strings, arrays, dictionaries, references |
| `pdf_file_object.{hpp,cpp}`            | File-structure entries: `Header`, `IndirectObject`, `Trailer`, `Xref` (tagged-union entries, `append`/`merge_hybrid`), `StartXref`, `Eof`, the `Entry` any-holder; `parse_xref_stream_table` and the `ObjectStream` payload wrapper |
| `pdf_file_parser.{hpp,cpp}`            | File-level reads on top of `ObjectParser`: indirect objects, xref, trailer, startxref, stream payloads, `seek_start_xref` |
| `pdf_filter.{hpp,cpp}`                 | Stream filter framework: `decode()` over the `/Filter`/`/DecodeParms` chain; ASCIIHex/ASCII85/LZW/Flate/RunLength decoders, TIFF/PNG predictors; image codecs returned undecoded (`DecodeResult::stopped_at_filter`) |
| `pdf_document_parser.{hpp,cpp}`        | `parse_document()`: xref/trailer chain → catalog → page tree; lazy object reads with cache; (deep) reference resolution |
| `pdf_encryption.{hpp,cpp}`             | Standard security handler: `Decryptor` (parse `/Encrypt`, authenticate, decrypt strings/streams; RC4, AES-128, AES-256) + a `standard_security` namespace of pure key/password algorithms for known-answer tests |
| `pdf_document.hpp`                     | `Document`: arena of `Element`s + `catalog` pointer |
| `pdf_document_element.hpp`             | Element structs: `Catalog`, `Pages`, `Page`, `Annotation`, `Resources`, `Font` |
| `pdf_cmap.{hpp,cpp}`                   | `CMap`: 1-byte glyph → UTF-16 `bfchar` map + string translation |
| `pdf_cmap_parser.{hpp,cpp}`            | `ToUnicode` CMap stream parser (`begincodespacerange`/`beginbfchar`/`beginbfrange`; only `bfchar` applied) |
| `pdf_graphics_operator.hpp`            | `GraphicsOperatorType` enum (full operator set) + `GraphicsOperator` (type + `Object` arguments) |
| `pdf_graphics_operator_parser.{hpp,cpp}` | Content-stream tokenizer: arguments (numbers/names/strings/arrays/dicts) then operator name |
| `pdf_graphics_state.{hpp,cpp}`         | `GraphicsState`: stack of `State` (general/path/text/color), `execute(op)` for the modelled subset |
| `pdf_file.{hpp,cpp}`                   | `abstract::PdfFile` wrapper; probes encryption at construction and implements `password_encrypted()`/`decrypt()`, carrying the authenticated `Decryptor` (not the password) so rendering needs no re-derivation |

Consumers outside the module: `open_strategy.cpp` (detection / engine
selection) and `html/pdf_file.cpp` (`create_pdf_service` — the HTML
translation described above, dispatched from `html::translate(PdfFile)` when
the file is not a `PopplerPdfFile`).

## Pipeline: how a `.pdf` becomes HTML

1. **Wiring.** `open_strategy` maps `FileType::portable_document_format` to
   `PdfFile` (the odr engine); `DecoderEngine::poppler` or the
   unknown-file-type fallback can yield a `PopplerPdfFile` instead when built
   with `ODR_WITH_PDF2HTMLEX`. `html::translate(PdfFile)` then picks the
   matching HTML service.

2. **Locate the xref.** `seek_start_xref` seeks to `EOF − 64` and scans lines
   for `startxref`; `read_start_xref` yields the offset of the most recent
   xref table. (`read_header` exists but `parse_document` does not call it —
   the `%PDF-` header is only checked by magic detection earlier.)

3. **Walk the trailer chain.** At each position, `read_xref_section`
   dispatches: a classic table (`read_xref` + `read_trailer`) or a
   cross-reference stream (an indirect object whose dictionary doubles as the
   trailer dict; payload decoded via the filter framework, entries via
   `parse_xref_stream_table`). A trailer `XRefStm` (hybrid file) is read
   next and fills entries the classic table lacks or marks free
   (`merge_hybrid`). Sections are merged into the accumulated table —
   `std::map::insert` keeps the first (newest) entry — then `Prev` is
   followed (cycle-guarded). The first (newest) trailer provides `Root`.

4. **Build the page tree.** `parse_catalog` → `parse_pages` recurses over
   `Kids` (dispatching on `Type` = `Pages`/`Page`). Each `Page` keeps its raw
   dictionary, its `Contents` reference(s), parsed `Resources` (the `Font`
   table; each font's `ToUnicode` CMap is parsed if present) and `Annots`
   (raw). Objects are fetched via `read_object`, which dispatches on the
   xref entry kind: used → seek + `read_indirect_object`; compressed → the
   owning object stream is decoded once, cached, and the member parsed from
   the cached payload; free/absent → null object with a warning. Parsed
   objects are cached by reference.

5. **Decode content.** For each page (depth-first over the tree), the
   `Contents` streams are read (`/Length`, possibly an indirect reference),
   decoded through their `/Filter` chain
   (`DocumentParser::read_decoded_stream`), and concatenated with a newline
   between streams (token-boundary join, ISO 32000-1 7.7.3.3).

6. **Execute and emit.** `GraphicsOperatorParser` tokenizes the stream;
   `GraphicsState.execute` updates the state stack. `T*` advances the text
   offset by `size + leading`; `Tj` emits a positioned `span` using
   `state.text.offset` (from `Td`/`TD`) and the `Tf` size, with glyphs
   translated through the font's CMap. The text and transform matrices are
   tracked but **not applied** to positioning.

---

# Design decisions

## Stream-based parsing with seeks, lazy object access

Everything is parsed off a `std::istream`/`std::streambuf` — no full-file
buffer. Random access (xref lookups, stream payloads) seeks; sequential
tokenizing uses single-character peek/bump (`geti`/`getc`/`bumpc`). Objects
are only parsed when referenced, and parsed `IndirectObject`s are cached by
reference, so shared objects (fonts, resources) are read once. Positions are
`std::uint32_t` (files ≥ 4 GiB are out of scope).

## `std::any`-based object model

`Object` holds its value in `std::any` with typed `is_*`/`as_*` accessors
(mirrors `oldms/`'s `Entry` pattern). Pros: one value type throughout parser,
document elements, and operator arguments. Cons: no exhaustive matching, RTTI
lookups, and copies are easy to make accidentally — `resolve_object_copy`
exists precisely because rvalue access proved fiddly (see the `TODO why rvalue
not working?` in `pdf_document_parser.cpp`).

## References are recognized by lookahead

`n g R` is plain integers until the `R` appears, so `read_array` and
`read_dictionary` patch references after the fact: in arrays, an `R` after a
value pops the previous two integers; in dictionaries, a second integer + `R`
after the value rewrites it (`TODO this seems hacky`). A standalone
`read_object` therefore returns the *id* integer of a reference — only
array/dictionary contexts and `read_object_reference` assemble real
references. Works for well-formed files; a known sharp edge.

## Element tree as an arena

`Document` owns all elements (`vector<unique_ptr<Element>>`); `Catalog`,
`Pages`, `Page`, … hold raw non-owning pointers plus their original
dictionary (`Element::object`), so unmodelled keys stay inspectable.
Navigation in consumers is by `dynamic_cast` over `kids` (the `Type` enum
exists but is not used for dispatch).

## Fail early on malformed structure, tolerate unknown content

Structural surprises **throw** `std::runtime_error`: missing `obj`/`endobj`/
`stream`/`endstream`/`xref`/`startxref` keywords, unexpected characters in
arrays/dictionaries/strings, an unknown object start, an unresolvable
`/Length`, an unknown page-tree element type, or stream exhaustion. Unknown
**content** is tolerated: unrecognized graphics operators are logged to stderr
and skipped, unmodelled operator types are ignored by `execute`, annotations
keep their raw dictionary, and CMap `codespacerange`/`bfrange` sections are
parsed past without effect. References to free or absent objects resolve to
the null object with a `Logger` warning (broken files reference freed
objects routinely); cross-reference-stream entries of unknown type are
treated as absent per 7.5.8.3.

## Debug output still in place

`html/pdf_file.cpp`, `pdf_graphics_state.cpp`, `pdf_graphics_operator_parser.cpp`
and `pdf_cmap_parser.cpp` print diagnostics (and one leftover `"hi"`
breakpoint marker) to stdout/stderr instead of using the engine's `Logger`.
This reflects the proof-of-concept state; it should move to `Logger` or be
removed. `DocumentParser` itself now takes an optional `Logger &` (default
`Logger::null()`; the HTML service passes its own) and routes its WP2
warnings through it — new diagnostics should do the same.

---

## Tests

- `test/src/internal/pdf/pdf_filter.cpp` — **assertion-based** coverage of
  the filter framework, all inputs inline strings (no fixture files): every
  decoder, predictors, chains, image-codec stop, `Crypt`/unknown-filter
  errors.
- `test/src/internal/pdf/pdf_file_object.cpp` — **assertion-based**, inline
  inputs only: cross-reference-stream entry decoding (field widths incl. 0,
  type default, big-endian fields, subsections, unknown types, error paths),
  `ObjectStream` header parsing and member lookup, `Xref::append` and
  `Xref::merge_hybrid` precedence.
- `test/src/internal/pdf/pdf_encryption.cpp` — **assertion-based**, inline
  vectors only: the standard security handler across R 2 (RC4-40), R 3
  (RC4-128), R 4 (AES-128/AESV2, incl. `EncryptMetadata false` and an
  owner-locked file) and R 6 (AES-256). Vectors come from the real fixtures
  (`Casio…`, `encrypted_fontfile3…`) and from `qpdf --encrypt` output frozen as
  literals — decrypting back to a known marker, so no test is circular through
  our own implementation and no fixture file ships. `crypto_util_test.cpp`
  covers the new MD5/RC4/SHA-384/512 primitives against public standard
  vectors.
- `test/src/internal/pdf/pdf_document_parser.cpp` — **assertion-based**
  whole-file tests over mini-PDFs assembled by the test-only
  `pdf_test_file_builder.{hpp,cpp}` (computes xref offsets/`startxref`, so
  tests show only the dictionaries; classic-table and
  uncompressed-xref-stream variants), plus inherited-page-attribute coverage
  (a multi-level `Pages` tree asserting per-page resolved
  `MediaBox`/`CropBox`/`Rotate`/`Resources`, override vs. inheritance, the
  `CropBox` ← `MediaBox` default and the missing-`MediaBox` US-Letter
  lenience path) and an end-to-end run of the classic fixture
  `odr-public/pdf/style-various-1.pdf` — page-tree walk and content-stream
  decoding, plus end-to-end decryption of the encrypted fixtures
  `odr-public/pdf/Casio_WVA-M650-7AJF.pdf` (RC4, empty password) and
  `odr-private/pdf/encrypted_fontfile3_opentype.pdf` (AES-256, password from
  `index.csv`; skipped when the private submodule is absent). The `odr-private`
  xref-stream/objstm/hybrid fixtures (`basic_text.pdf`, `geneve_1564.pdf`,
  `test_fail.pdf`, `Kayla….pdf`, `svg_background…issue402.pdf`,
  `Core_v5.1.pdf`, `onepage.pdf`) were verified manually when WP2 landed but
  are not pinned in unit tests — the submodule is not always available; the
  generic all-test-files jobs cover them. Also still contains the original
  print-everything smoke test.
- `test/src/internal/pdf/pdf_file_parser.cpp` — sequential `read_entry` walk
  (smoke) and assertion-based xref/trailer/root navigation over
  `odr-public/pdf/style-various-1.pdf`.

There is no assertion-based coverage of the tokenizer (escapes, references,
hex strings), the CMap, or the HTML output.

## Out of scope / open work

The staged roadmap lives in [`PLAN.md`](PLAN.md); the gap list (partly from
[`README.md`](README.md)):

- **Linearized files** are not handled specially (the tail-first read usually
  still works, but the hint streams are ignored).
- **Xref recovery** for broken files (garbage `startxref`, wrong offsets) is
  stage 0 WP5 — structural damage still throws.
- **Stream filters**: framework done (stage 0 WP1, see above); the remaining
  gap is the image codecs (decoded in stage 4, currently passed through
  encoded). Decryption is applied before filter decoding by `DocumentParser`
  rather than through the `Crypt` filter, so the filter framework still only
  treats `Crypt` as `Identity`.
- **Encryption**: standard security handler done (stage 0 WP4, see above —
  RC4, AES-128, AES-256). Remaining edge cases, deferred until a real file
  needs them: per-stream `/Crypt` filter `Name` overrides, the
  `EncryptMetadata false` metadata-stream `Identity` special case, and `Perms`
  (Algorithm 13) key validation; the public-key security handler and R 5 are
  out of scope.
- **Text fidelity**: text/CTM matrices are tracked but not applied; no glyph
  widths/advances, so `TJ` spacing, `'`/`"`, font metrics, and
  per-character positioning are missing — only `Td`/`TD`/`T*` plus `Tj`
  render. Font selection (`font-family`) is not emitted.
- **CMap coverage**: only single-byte `bfchar`; `bfrange` and
  `codespacerange` are skipped, multi-byte codes are not supported, and fonts
  without `ToUnicode` fall back to identity bytes (the README discusses the
  glyph-ID → Unicode problem at length).
- **Graphics**: paths, fills/strokes, clipping, images (XObjects and inline),
  shading, transparency — all parsed past, none rendered.
- **Annotations** are collected but their content is not interpreted.
- **Engineering**: replace the remaining stdout/stderr debugging with
  `Logger`, remove the leftover `"hi"` marker in `html/pdf_file.cpp`, turn
  the remaining smoke tests into assertion-based tests, and revisit the
  reference-by-lookahead parsing and `read_stream(-1)` fallback.
