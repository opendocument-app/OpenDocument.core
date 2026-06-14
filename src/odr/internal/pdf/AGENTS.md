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
text spans per `Tj`, pages sized from `MediaBox`. Encrypted files are decrypted
(RC4, AES-128, AES-256). No graphics, no images, no font files. Experimental and
not production-quality — the HTML path still contains debug `std::cout` output.

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
  bytes). Glyph-name `/Encoding`, predefined CJK CMaps and embedded-font
  fallbacks are still stage 1.2–1.4.
- **Content streams**: the full graphics-operator vocabulary is tokenized;
  `GraphicsState` executes a subset (state stack `q`/`Q`, matrices `cm`/`Tm`,
  line parameters, text state `Tc`/`Tw`/`Tz`/`TL`/`Tf`/`Tr`/`Ts`, text
  positioning `Td`/`TD`, grey/RGB/CMYK colors, glyph metrics `d0`/`d1`). Unknown
  operators are logged to stderr and skipped.
- **HTML**: one `document.html` view; each page is a `div` sized from `MediaBox`
  (points → inches), each `Tj` becomes an absolutely positioned `span` at the
  text-state offset with `font-size` from `Tf` and the CMap-translated text.
  `TJ`/`'`/`"` are recognized but only printed to stdout, not rendered.

## Module layout

| File (`pdf/`)                          | Role                                                  |
|----------------------------------------|-------------------------------------------------------|
| `pdf_object.{hpp,cpp}`                 | Object model: `Object` (`std::any`-based variant), `Array`, `Dictionary`, `Name`, `StandardString`/`HexString`, `ObjectReference`; `to_stream`/`to_string` dumping |
| `pdf_object_parser.{hpp,cpp}`          | Tokenizer over `std::streambuf`: whitespace/lines, numbers, names, strings, arrays, dictionaries, references |
| `pdf_file_object.{hpp,cpp}`            | File-structure entries: `Header`, `IndirectObject`, `Trailer`, `Xref` (tagged-union entries, `append`/`merge_hybrid`), `StartXref`, `Eof`, the `Entry` any-holder; `parse_xref_stream_table` and the `ObjectStream` payload wrapper |
| `pdf_file_parser.{hpp,cpp}`            | File-level reads on top of `ObjectParser`: indirect objects, xref, trailer, startxref, stream payloads, `seek_start_xref` |
| `pdf_filter.{hpp,cpp}`                 | Stream filter framework: `decode()` over the `/Filter`/`/DecodeParms` chain; ASCIIHex/ASCII85/LZW/Flate/RunLength decoders, TIFF/PNG predictors; image codecs returned undecoded (`DecodeResult::stopped_at_filter`) |
| `pdf_document_parser.{hpp,cpp}`        | `parse_document()`: xref/trailer chain → catalog → page tree; lazy object reads with cache; (deep) reference resolution |
| `pdf_encryption.{hpp,cpp}`             | Standard security handler: `Authenticator` (parse `/Encrypt`, authenticate password → `Decryptor`) and `Decryptor` (decrypt strings/streams; RC4, AES-128, AES-256), plus a `standard_security` namespace of pure key/password algorithms for known-answer tests |
| `pdf_document.hpp`                     | `Document`: arena of `Element`s + `catalog` pointer |
| `pdf_document_element.hpp`             | Element structs: `Catalog`, `Pages`, `Page`, `Annotation`, `Resources`, `Font` |
| `pdf_cmap.{hpp,cpp}`                   | `CMap`: 1-byte glyph → UTF-16 `bfchar` map + string translation |
| `pdf_cmap_parser.{hpp,cpp}`            | `ToUnicode` CMap stream parser (`begincodespacerange`/`beginbfchar`/`beginbfrange`; only `bfchar` applied) |
| `pdf_encoding.{hpp,cpp}`               | Simple-font `/Encoding` → Unicode: `BaseEncoding` tables, `/Differences` overlay (`Encoding`), glyph-name → Unicode via AGL + `uniXXXX`/`uXXXXXX` (stage 1.2) |
| `pdf_encoding_data.{hpp,cpp}`          | **Generated** (`tools/pdf/generate_encoding_data.py`): base-encoding tables + the Adobe Glyph List as a name-sorted array |
| `pdf_graphics_operator.hpp`            | `GraphicsOperatorType` enum (full operator set) + `GraphicsOperator` (type + `Object` arguments) |
| `pdf_graphics_operator_parser.{hpp,cpp}` | Content-stream tokenizer: arguments then operator name |
| `pdf_graphics_state.{hpp,cpp}`         | `GraphicsState`: stack of `State` (general/path/text/color), `execute(op)` for the modelled subset |
| `pdf_file.{hpp,cpp}`                   | `abstract::PdfFile` wrapper; probes encryption at construction and implements `password_encrypted()`/`decrypt()`, carrying the authenticated `Decryptor` (not the password) so rendering needs no re-derivation |

Consumers outside the module: `open_strategy.cpp` (detection / engine
selection) and `html/pdf_file.cpp` (`create_pdf_service`).

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
   `Contents` reference(s), parsed `Resources` (the `Font` table; each font's
   `ToUnicode` CMap is parsed if present) and `Annots` (raw). `read_object`
   dispatches on the xref entry kind: used → seek + `read_indirect_object`;
   compressed → owning object stream decoded once, cached, member parsed from
   the cached payload; free/absent → null with a warning. Parsed objects cached
   by reference.
5. **Decode content.** Per page (depth-first), the `Contents` streams are read,
   decoded through their `/Filter` chain (`read_decoded_stream`), concatenated
   with a newline between streams.
6. **Execute and emit.** `GraphicsOperatorParser` tokenizes; `GraphicsState`
   updates the state stack. `T*` advances the text offset by `size + leading`;
   `Tj` emits a positioned `span` using `state.text.offset` and the `Tf` size,
   glyphs translated through the font's CMap. The text and transform matrices
   are tracked but **not applied** to positioning.

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

**Debug output still in place.** `html/pdf_file.cpp`, `pdf_graphics_state.cpp`,
`pdf_graphics_operator_parser.cpp` and `pdf_cmap_parser.cpp` print diagnostics
(and one leftover `"hi"` breakpoint marker) to stdout/stderr instead of
`Logger`. Proof-of-concept residue; should move to `Logger` or be removed.
`DocumentParser` itself takes an optional `Logger &` (default `Logger::null()`)
and routes its warnings through it — new diagnostics should do the same.

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
  stream). End-to-end: the classic fixture
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
  `base_encoding_from_name`, glyph-name → Unicode via the AGL (incl. a ligature
  and the `name.suffix` form) and the algorithmic `uniXXXX`/`uXXXXXX` forms,
  `Encoding::translate_string` with a base encoding, a `/Differences` override,
  and the WinAnsi-vs-Standard `0x27` divergence (stage 1.2).

No assertion-based coverage of the tokenizer (escapes, references, hex strings)
or the HTML output.

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

## Stage 1 — text extraction: the code → Unicode chain

PDF strings are **character codes**; per font, walk this chain and record
per-code Unicode (or "unknown", which stage 3 handles). The stage is **too
large for one change** — it bundles work of very different size and dependency,
so it is split into the sub-stages below. They are independently useful and
ordered by corpus frequency; each is its own branch/PR off this roadmap. Sub-
stage 1.1 has landed; **1.2 is the current work** (branch
`pdf-encoding-to-unicode`); 1.4 is blocked on stage 3 and stays deferred until
then.

### 1.1 — `ToUnicode` CMap: multi-byte codes, `bfrange`, multi-char targets — **done**

The narrowest, most self-contained chunk: it only extends the existing `CMap`
(`pdf_cmap.{hpp,cpp}`) and its parser (`pdf_cmap_parser.cpp`), with no new data
tables and no new font plumbing. Today the map is single-byte
(`std::unordered_map<char, char16_t>`), `read_bfchar` warns on multi-byte glyphs
/ multi-char targets via `std::cerr`, and `read_bfrange` / `read_codespacerange`
are empty `// TODO` stubs.

Scope of this one change:
- **Code keys become multi-byte.** Key the map on the full character code, not a
  single `char` (e.g. a `std::uint32_t` code + the byte length, or a
  `std::string` code). `codespacerange` defines the code byte-lengths; record the
  ranges so `translate_string` can chunk a string into codes of the right width
  (most `ToUnicode` CMaps are fixed 1- or 2-byte, but the ranges are authoritative).
- **`bfrange`.** Both forms: `<lo> <hi> <dst>` (increment the last UTF-16 unit of
  `dst` across the code range, per spec only the low byte) and
  `<lo> <hi> [ <dst0> <dst1> … ]` (array of per-code destinations).
- **Multi-character targets.** A code may map to a *string* of UTF-16 units
  (ligatures, e.g. `ﬁ` → `f` `i`); `map_bfchar`'s value type widens from
  `char16_t` to `std::u16string` (or a small-string equivalent).
- **`translate_string`** chunks the input by the codespace widths, looks up each
  code, and falls back to the identity/​byte value (today's behaviour) on a miss
  — keeping the "unknown" path for later sub-stages to refine.
- Replace the two `std::cerr` warnings with the module's `Logger` (thread one in,
  as `DocumentParser` already does), or drop them once the cases are handled.

Tests (assertion-based, inline CMap strings — no fixtures): a 2-byte
`codespacerange`; `bfrange` increment form and array form; a multi-char `bfchar`
target; mixed widths; a miss falling back to identity. This matches the existing
inline-string test convention for the module.

Out of scope for 1.1: anything needing `/Encoding`, the AGL, predefined CMaps,
or font-file reading — those are 1.2–1.4.

### 1.2 — simple-font encoding → Unicode — **in progress**

`/Encoding` base (WinAnsi/MacRoman/Standard) + `/Differences` → glyph names →
Unicode via the Adobe Glyph List (incl. `uniXXXX`/`uXXXXXX` names). Carries the
data weight of the three base-encoding tables **and** the full AGL (~4,300
entries) — a generated data file. Own branch.

The chain, per simple font that has no `ToUnicode` CMap: a 1-byte code → glyph
name (base encoding, overlaid with `/Differences`) → Unicode (AGL lookup, or the
algorithmic `uniXXXX`/`uXXXXXX` forms). `translate_string` walks a code string
byte by byte; an unmapped name yields "no Unicode" (empty), left for stage 1.5.

**Data as committed generated source.** `tools/pdf/generate_encoding_data.py`
emits `pdf_encoding_data.{hpp,cpp}` (the base-encoding tables + the AGL as a
name-sorted array for binary search); the build only compiles the result, so
there is no build-time codegen dependency. Re-run the script with Adobe's
`glyphlist.txt` to populate the full AGL.

Scope landed so far (foundation/scaffolding):
- `pdf_encoding.{hpp,cpp}`: `BaseEncoding`, `base_encoding_table` /
  `base_encoding_from_name`, `glyph_name_to_unicode` (AGL + `uniXXXX`/`uXXXXXX`),
  and the `Encoding` class (base + `/Differences` → `translate_string`).
- `/Encoding` parsing wired into `parse_font` (`parse_encoding`): a base name, or
  a dictionary with `/BaseEncoding` + `/Differences`. Stored on `Font::encoding`
  (a `std::optional<Encoding>`).
- The generator + a committed seed data file (basic Latin + a few extras) so the
  module compiles and the inline tests pass.

Remaining (follow-up commits on this branch):
- Populate the **full** base-encoding tables (ISO 32000-1 Annex D: the complete
  Latin set, MacRoman, the Latin-1 upper half of WinAnsi) and the **full AGL**
  via the generator.
- Use `Font::encoding` as the fallback in the HTML text path
  (`html/pdf_file.cpp`) when the font has no `ToUnicode` CMap — currently parsed
  and stored but not yet consulted during emission.
- Symbolic fonts / the "built-in encoding" default (no `/BaseEncoding`) need the
  font program — defer to stage 1.4; for now StandardEncoding is the default base.

### 1.3 — composite (Type0/CID) fonts

`Identity-H/V` plus the predefined CMaps (CJK); map CID → Unicode via the CID
system info where defined. The predefined CMaps are large external data sets —
the heaviest data chunk of the stage. Own branch.

### 1.4 — embedded-font fallback — **deferred (needs stage 3)**

Reverse the TrueType `cmap`; read glyph names from Type1/CFF charstrings.
Explicitly depends on stage 3's font *reading*, so it cannot start until that
machinery exists.

### 1.5 — "no Unicode" runs + `/ActualText`

Nothing applies → mark the run "no Unicode" for stage 3's re-encoding.
`/ActualText` (tagged PDFs, ligatures) overrides the whole chain for extraction.
Small, but rides on the run/state plumbing the sub-stages above introduce.

## Stage 2 — text positioning & metrics

Independent of Unicode work; fixes layout even with today's partial CMaps.

- Apply the full transform: text matrix × CTM (both tracked in `GraphicsState`
  but never applied), text rise, horizontal scaling.
- **Glyph advances**: `/Widths` + `/MissingWidth` (simple), `/W` + `/DW` (CID),
  char/word spacing, the numeric adjustments in `TJ` — so `TJ`, `'`, `"` finally
  render and `Tj` runs land correctly.
- **Form XObjects** (`Do` on a `/Form`): recursive content-stream execution with
  scoped `/Resources` and the form matrix. Many producers put most page content
  inside forms, and tiling patterns (stage 4) and annotation appearances
  (stage 5) run on the same machinery — a structural prerequisite.
- **Text render modes** (`Tr`): mode 3 (invisible text, OCR-over-scan) must stay
  selectable but unpainted; stroke/clip modes (1–2, 4–7) need graceful
  degradation.
- **Space inference**: PDFs routinely encode no spaces; insert them from
  glyph-gap heuristics (as pdf2htmlEX does) so copy/paste and search work.
- Layout side of bidi (RTL run ordering) and vertical writing (Identity-V/CJK).
- HTML mapping decision: per-run spans with CSS `transform` (cheap, breaks on
  heavy kerning) vs. per-glyph positioning (exact, verbose) — likely per-run
  with a kerning threshold that splits runs, like pdf2htmlEX.

## Stage 3 — fonts in HTML

Needed for visual fidelity regardless of text extraction.

**Decision (2026-06): in-house, no FontForge.** pdf.js proves complete font
conversion is doable without a font library; pdf2htmlEX uses FontForge at the
cost of a notoriously heavy build. No trimmed off-the-shelf alternative does
what we need (FreeType/stb_truetype are read-only; hb-subset can only subset
along the *existing* `cmap`, so it cannot inject the PUA mappings below).
Expected ~5–8k lines of focused C++ — on the order of an `oldms/` module.
Reading (SFNT tables, CFF charsets) is the easy part and is needed by stage 1.4
anyway.

**Architecture: IR for facts, pass-through for glyphs.** No glyph-level font IR:
decompiling and recompiling outlines is the FontForge model — loses hinting,
risks fidelity, and with one output format (SFNT) the M×N payoff never
materializes. Glyph data (outlines, hinting, charstrings) passes through
byte-for-byte; even Type1 → Type2 charstrings is a direct sibling-format
translation. What *is* shared: a thin `FontProgram`-style interface — per-flavor
readers producing the facts every consumer needs (glyph count, glyph → Unicode,
advance widths, units-per-em, name, bbox, symbolic flag) with raw bytes kept
alongside. Stage 1.4 reads Unicode from it, the OTF wrap synthesizes
`head`/`hhea`/`hmtx`/`OS/2` from it, the re-encoder assigns PUA code points from
its glyph count.

**Intermediate milestone — fonts as first-class library citizens.** Before
wiring fonts into PDF output, ship them standalone:
- **Font files as a `DecodedFile` type** (precedent: `SvmFile`, `ImageFile`):
  `FileType` entries + magic detection (SFNT `0x00010000`/`OTTO`/`wOFF`), a
  `FontFile` category, and `html::translate(FontFile)` emitting a **specimen
  page** — name/metrics header plus a glyph grid, font served via `@font-face`.
  Keep the UI at "specimen page"; no font-editor scope creep.
- The glyph grid must show **every** glyph, including ones no `cmap` reaches —
  which forces building the PUA re-encoding, table-directory rebuild, and OTF
  wrap *first*, against a directly viewable deliverable with font-only tests.
- **In parallel: PDF as a container.** Expose embedded fonts as an
  `abstract::Filesystem` (`/fonts/F1.ttf`, …) and reuse the filesystem HTML
  service (as for ZIP/CFB). Doubles as the corpus harvester.

Sub-stages, ordered by corpus frequency, each independently useful:
1. **TrueType** (`FontFile2`, CIDFontType2 — bulk of modern PDFs): serve nearly
   as-is via `@font-face`; implement the `cmap` rewrite (format-4/12 subtable,
   splice the table directory, recompute `head.checkSumAdjustment`).
2. **Bare CFF** (`FontFile3`/Type1C): wrap into an OTF container by synthesizing
   the ~8 required tables; take advance widths from `/Widths`/`/W` rather than
   interpreting charstrings.
3. **Type1** (`FontFile` — older docs, pdfTeX/academic PDFs): `eexec`
   decryption, Type1 → Type2 charstrings, build a CFF, reuse sub-stage 2. The
   hardest single piece but precisely specified (Adobe T1 spec; pdf.js as
   reference).
4. **Type3** (drawing procedures, no font file — scientific plots) → SVG glyphs
   reusing stage 4's path rendering; plus **non-embedded fonts**: substitute the
   standard 14 + common names with CSS fallbacks + metrics from `/Widths`.

Mechanisms and guards:
- **Re-encoding for unmapped glyphs** (the general workaround): rewrite the
  `cmap` so deterministic PUA code points (`U+E000 + glyph index`) map to the
  glyphs, emit those in the HTML, mark such runs non-extractable
  (`user-select: none`, `aria-hidden`). Display correct; copy/search knowingly
  garbage. Option: re-encode *all* fonts this way (pdf2htmlEX's choice) for one
  uniform pipeline.
- **Broken-font long tail**: real embedded fonts are routinely malformed, and
  browsers run web fonts through a sanitizer (OTS) that silently rejects them.
  Regenerating the table directory (which the re-encode/wrap does anyway) covers
  most of it; start strict, add repair heuristics as real files demand. CI gate:
  run **OTS** over every produced font (test-time only); optionally FreeType as
  a second oracle. Neither ships in the product.

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
  `bfchar`, both `bfrange` forms, multi-char targets — stage 1.1). Still open:
  fonts *without* a `ToUnicode` stream (glyph-name `/Encoding`, predefined CJK
  CMaps, embedded-font reverse maps) fall back to identity bytes until stages
  1.2–1.4.
- **Annotations** are collected but their content is not interpreted (stage 5).
- Revisit the reference-by-lookahead parsing and `read_stream(-1)` fallback.
