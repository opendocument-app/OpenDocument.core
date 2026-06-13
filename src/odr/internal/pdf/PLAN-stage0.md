# Stage 0 — file-format compatibility: detailed design

Detailed design for stage 0 of [`PLAN.md`](PLAN.md). Spec references are to
the offline ISO 32000-1:2008 copy
(`offline/documentation/PDF/PDF32000_2008/`, section/table numbers as
cited); AES-256 references (WP4) are to the offline ISO 32000-2:2020 copy
(`offline/documentation/PDF/ISO_32000-2/`).

**Goal.** Files written by modern producers (PDF 1.5+ xref/object streams,
filtered streams, inherited page attributes, encrypted files) get through
`parse_document` and produce the same proof-of-concept HTML that classic
files do today. No rendering improvements — parsing only.

**Exit criteria.** Every fixture in the corpus below parses (or fails with a
*deliberate* diagnostic, e.g. unsupported encryption), with assertion-based
tests per work package.

**Testing approach: inline strings first, fixture files second.** Everything
here parses from `std::istream`, so tests can feed an `std::istringstream`
over an inline string literal — the input is then *visible in the test
source* next to the assertions, instead of hidden in a binary fixture.
Prefer that wherever the input can be written by hand: individual filter
payloads, xref/objstm sections, object syntax, crafted mini-PDFs. The one
mechanical obstacle for whole-file inputs is byte offsets (xref entries,
`startxref`): hand-maintaining them makes literals brittle, so add a small
test-only builder that assembles a PDF from object snippets and computes the
classic xref table / `startxref` itself — the test then shows only the
interesting dictionaries. Real fixture files remain for what strings can't
express readably (compressed/binary payloads, encryption, real-producer
quirks) and as end-to-end coverage.

---

## Work packages and order

| WP  | What | Depends on |
|-----|------|------------|
| WP1 | Filter framework — **done** (2026-06, `pdf_filter.{hpp,cpp}`) | — |
| WP2 | Xref streams, object streams, hybrid files — **done** (2026-06) | WP1 (xref streams are Flate-compressed) |
| WP3 | Inherited page attributes — **done** (2026-06) | — (parallel to WP1/WP2) |
| WP4 | Encryption (standard security handler) | WP1, WP2 (Encrypt dict can live anywhere; key needs trailer `ID`) |
| WP5 | Xref recovery for broken files | WP2; explicitly *after* stage 0 ships |

WP1+WP2 unblock the majority of modern files and should land first; WP3 is
small and independent; WP4 trails (as in the high-level plan).

---

## WP1 — filter framework

Spec: 7.4 General + Table 6 (filter list); Table 5 (`Filter`, `DecodeParms`
stream-dict entries, filter *chains* as arrays); 7.4.2 ASCIIHexDecode; 7.4.3
ASCII85Decode; 7.4.4 LZW/Flate, Table 8 (`Predictor`, `Colors`,
`BitsPerComponent`, `Columns`, `EarlyChange`), Tables 9–10 (predictors);
7.4.5 RunLengthDecode.

### Design

New files `pdf_filter.{hpp,cpp}`:

```cpp
struct DecodeResult {
  std::string data;
  // set when decoding stopped at an image codec (DCTDecode, JPXDecode,
  // CCITTFaxDecode, JBIG2Decode): the codec name and its DecodeParms,
  // with `data` holding the still-encoded payload for that codec.
  std::optional<std::string> stopped_at_filter;
  Object stopped_at_parms;
};

// `filter` / `parms` are the (already reference-resolved) values of
// /Filter and /DecodeParms: name-or-array and dict-or-array respectively.
DecodeResult decode(const Object &filter, const Object &parms,
                    std::string data);
```

- Caller (`DocumentParser::read_object_stream`) resolves indirect `/Filter`
  / `/DecodeParms` values before calling; the filter module stays free of
  document/xref knowledge. (For xref streams the spec *requires* these to be
  direct — 7.5.8.2 — so the bootstrap path needs no resolution.)
- Implemented decoders, applied left-to-right through the chain:
  - **FlateDecode** — existing `crypto::util::zlib_inflate` (Crypto++).
  - **LZWDecode** — hand-rolled (~100 lines): 9→12-bit codes, clear-table
    256, EOD 257, honour `EarlyChange` (default 1).
  - **ASCIIHexDecode** — whitespace ignored, `>` is EOD, odd digit count
    implies trailing 0.
  - **ASCII85Decode** — `z` shortcut, `~>` EOD, partial-group rule.
  - **RunLengthDecode** — trivial; included for completeness (length byte
    0–127: copy n+1; 129–255: repeat next byte 257−n; 128: EOD).
  - **Predictors** (post-pass after Flate/LZW when `Predictor > 1`):
    TIFF predictor 2 and PNG predictors 10–15. PNG: per-row algorithm tag
    byte, row size from `Columns`×`Colors`×`BitsPerComponent` (defaults
    1/1/8), byte-wise prediction regardless of component layout (7.4.4.4).
    Used by virtually every xref stream in the wild (typically
    `/Predictor 12 /Columns <sum of W>`).
  - **Image codecs** (`DCTDecode`, `JPXDecode`, `CCITTFaxDecode`,
    `JBIG2Decode`) — never decoded; decoding stops and returns
    `stopped_at_filter` so stage 4 can hand the payload to `<img>`/PNG
    encoding later.
  - **Crypt** — handled by WP4 (per-stream crypt filter selection); until
    then, `Identity` passes through, anything else throws.
- No `/Filter` entry → identity.
- Replace today's two unconditional-inflate call sites:
  `pdf_document_parser.cpp:34` (ToUnicode CMap) and `html/pdf_file.cpp:125`
  (page content) — both via a new
  `DocumentParser::read_decoded_stream(const IndirectObject &)`.

### Edge cases / lenience

- `DecodeParms` array entries may be `null` for filters without parameters.
- Producers sometimes write the deprecated abbreviations (`Fl`, `AHx`,
  `A85`, `LZW`, `RL`, `DCT`, `CCF`) from the *inline image* table for
  regular streams — accept them.
- `/Length` lies in real files; the existing scan-to-`endstream` fallback
  stays as the recovery path, now applied *before* decoding.

### Tests

`test/src/internal/pdf/pdf_filter.cpp`, assertion-based, no PDF parsing and
no fixture files in the loop — every input is an inline string: per-filter
vectors (incl. the spec's ASCII85+LZW example stream from 7.4, Example 3 —
encoded input and expected plaintext side by side in the test), predictor
rows for PNG Up/Sub/Paeth and TIFF 2, chain decoding, EOD/garbage error
behaviour. Flate inputs are the one binary case: produce them with a
one-line zlib-compress helper at test runtime rather than embedding bytes.

---

## WP2 — xref streams, object streams, hybrid files

**Done** (2026-06). Implemented as designed, with deviations noted inline:
the entry-table decoder and `ObjectStream` live in `pdf_file_object.{hpp,cpp}`
(pure data transforms, no istream), the hybrid combine is
`Xref::merge_hybrid`, `read_xref_section`/`load_object_stream` are private
`DocumentParser` methods, the `Prev` walk gained a cycle guard, and
`read_indirect_object` now accepts `obj` without a trailing newline (the
`basic_text.pdf`/`test_fail.pdf` producer keeps the dictionary on the same
line). `DocumentParser` takes an optional `Logger &` for the new warnings.

Spec: 7.5.7 Object Streams + Table 16 (`N`, `First`, `Extends`); 7.5.8
Cross-Reference Streams + Table 17 (`Size`, `Index`, `Prev`, `W`) + Table 18
(entry types 0/1/2); 7.5.8.4 hybrid-reference files + Table 19 (`XRefStm`).

### Xref model change (`pdf_file_object.hpp`)

`Xref::Entry` becomes a tagged union (today: `{position, in_use}`):

```cpp
struct Xref {
  struct FreeEntry { std::uint32_t next_free_id; std::uint16_t next_generation; };
  struct UsedEntry { std::uint32_t position; };                      // type 1 / classic n
  struct CompressedEntry { std::uint32_t stream_id; std::uint32_t index; }; // type 2
  // Entry: std::variant<FreeEntry, UsedEntry, CompressedEntry>
  using Table = std::map<ObjectReference, Entry>;
  Table table;
  void append(const Xref &);  // unchanged: first (newest) entry wins
};
```

Compressed objects have generation 0 by definition (7.5.7), so the
`ObjectReference` key keeps working.

### Parsing (`pdf_file_parser` / `pdf_document_parser`)

`parse_document`'s trailer-chain walk becomes representation-agnostic. Per
section, produce `(Xref, Dictionary trailer_dict)`:

1. Seek to the section offset. If the next token is the keyword `xref` →
   classic path (existing `read_xref` + `read_trailer`).
2. Otherwise it must be an indirect object → read it; it is the
   cross-reference stream; its dict doubles as the trailer dict (`Root`,
   `Info`, `Encrypt`, `ID`, `Prev`, `Size` all live there). Don't insist on
   `/Type /XRef` beyond a warning — be lenient.
3. Decode the stream via WP1 (xref streams are never encrypted — 7.5.8.2 —
   so this is safe pre-encryption). Parse entries:
   - `W` = array of 3 field widths (PDF 1.5–1.7 always 3); width 0 → field
     absent → default (type defaults to **1**, type-1 generation defaults
     to 0). Fields are big-endian.
   - Subsections from `Index` (pairs `first count`), default `[0 Size]`.
   - Type 0 → `FreeEntry`, type 1 → `UsedEntry` (offset), type 2 →
     `CompressedEntry` (object-stream number, index). Unknown types →
     treat the object as free/null per 7.5.8.3, don't throw.
4. Chain handling (7.5.8.4 mandates the lookup order *classic section →
   its `XRefStm` → `Prev` section*): build a per-section table first —
   classic entries, then XRefStm entries filling in everything the classic
   table left out **or marked free** (hidden objects are free entries in a
   classic table but live entries in the xref stream; the spec says the
   reader shall find them in the stream and ignore the free entry) — then
   `append` that section table to the accumulated table (first/newest
   wins, unchanged), and finally follow `Prev`.
5. First (newest) trailer dict provides `Root`/`Encrypt`/`ID` as today.

### Object streams (`DocumentParser::read_object`)

`read_object` dispatches on the entry kind:

- `UsedEntry` → seek + `read_indirect_object` (today's path).
- `CompressedEntry` → load the object stream:
  1. `read_object({stream_id, 0})` → must have a stream; decode via WP1.
  2. Parse the header: `N` pairs of `obj_id offset` (plain integers,
     whitespace-separated), offsets relative to `First`.
  3. Cache decoded payload + offset table in
     `m_object_streams: map<uint32_t, ObjectStreamCache>` so N objects cost
     one decode.
  4. Parse the object at `First + offset[index]` with `ObjectParser` over an
     `std::istringstream`. Objects are bare values (no `n g obj` wrapper,
     no streams allowed inside — 7.5.7). Sanity-check the id at `index`
     matches the requested id; prefer the offset table's id on mismatch
     (warn).
  - The standalone-reference quirk of `ObjectParser::read_object` (a
    top-level `n g R` reads as an integer) is acceptable here: the spec
    forbids an objstm member from being just a reference (7.5.7).
  - `Extends` (collections) needs no special handling for reading — each
    stream is self-contained; ignore the link.
- `FreeEntry` / absent → null object (warn), instead of today's
  `map::at` throw — broken files reference freed objects routinely.

Wrap the result in `IndirectObject` as usual (no `stream_position`).

### Tests

Unit tests on inline strings — each piece of the machinery is testable in
isolation, below whole-file level, where the input stays hand-writable:

- **Xref-stream entry decoding**: feed the decoder the already-decoded
  binary table (a short `std::string` of escaped bytes, e.g. the spec's
  `W [1 2 1]` example from 7.5.7 Example 4) plus a hand-written `W`/`Index`
  — no compression involved; covers widths of 0, type default 1, big-endian
  fields, multiple subsections.
- **Object-stream member parsing**: the *decoded* objstm payload is plain
  text (`"11 0 12 547 ... << /Type /Font ... >>"`) — test header parsing
  and member lookup directly on an inline literal, no Flate, no file.
- **Hybrid/`Prev` merge semantics**: construct `Xref` tables in code and
  assert the merge precedence (classic → XRefStm fills free entries →
  `Prev`); no parsing at all.
- **Whole mini-files** via the offset-computing test builder (see testing
  approach above): a two-object classic PDF, the same with the table
  replaced by an uncompressed (`/Filter` absent) xref stream — uncompressed
  xref streams are legal, which keeps even the stream payload a readable
  escaped-byte literal.

Fixture files for end-to-end (page count, every xref entry resolvable,
content streams decode) — these are real-producer output that strings can't
reproduce honestly:

- `odr-public/pdf/style-various-1.pdf` — classic table (regression).
- `odr-private/pdf/basic_text.pdf`, `geneve_1564.pdf`, `test_fail.pdf`,
  `Kayla….pdf`, `svg_background…issue402.pdf`, `Core_v5.1.pdf` — xref +
  object streams (verified by inspection: all contain `/ObjStm` and
  `/Type /XRef`).
- `odr-private/pdf/onepage.pdf` — **hybrid** file (`XRefStm` in trailer).

---

## WP3 — inherited page attributes

**Done** (2026-06). Implemented as designed: an `InheritedAttributes`
accumulator is threaded through `parse_pages`/`parse_page` (no `Parent`
walk); `Page` carries the resolved `media_box`/`crop_box`/`rotate` plus its
resolved `resources`; the HTML page-size read now uses `Page::media_box`.
Deviation: `rotate` is stored as a normalized `Integer` on `Page` (not the
raw `Object`); `DocumentParser` gained a public `logger()` accessor so the
free parse helpers can warn.

Spec: 7.7.3.3 Table 30 — inheritable entries are exactly **`Resources`,
`MediaBox`, `CropBox`, `Rotate`** (everything else, incl. `AA`, is
explicitly not inheritable); 7.7.3.4 inheritance semantics.

### Design

During the `parse_pages` recursion, thread an accumulator instead of
consulting `Parent` pointers (no cycle risk, no re-reads):

```cpp
struct InheritedAttributes {
  Object resources;  // dict (possibly via reference — resolve lazily)
  Object media_box;  // rectangle array
  Object crop_box;   // rectangle array
  Object rotate;     // integer
};
```

- Each `Pages` node overlays its own values before recursing; `parse_page`
  resolves: page dict value → inherited value → default.
- Defaults / lenience: `CropBox` ← `MediaBox`; `Rotate` ← 0, normalized to
  {0,90,180,270} (spec: multiple of 90; real files contain 360, negatives);
  `MediaBox` missing everywhere (required-inheritable, so malformed) →
  US Letter `[0 0 612 792]` + `Logger` warning rather than throw;
  `Resources` missing → empty dict + warning (7.8.3 note: omission means
  inherit; absence everywhere is common in minimal files).
- `Page` element gets the *resolved* `media_box`/`crop_box`/`rotate` fields;
  `parse_resources` is fed the resolved resources object. The raw dict stays
  available via `Element::object` as today.
- HTML side keeps using `MediaBox` for page size for now (`CropBox`/`Rotate`
  application is stage 2 layout work); stage 0 only guarantees the values
  are *present and correct* on `Page`.

### Tests

This WP needs no fixture file at all: inheritance is pure dictionary logic,
so an inline mini-PDF (via the test builder) is the clearest input — a root
`Pages` node carrying `MediaBox`/`Resources`/`Rotate`, one inner `Pages`
level overriding `Rotate`, and two `Page` leaves (one overriding `MediaBox`,
one inheriting everything); the whole page tree fits in ~15 readable lines
of test source. Assert the resolved values per page, the `CropBox` ←
`MediaBox` default, and the missing-`MediaBox` lenience path. Real-world
cover comes free from the WP2 corpus run (e.g.
`svg_background_with_page_rotation_issue402.pdf` for rotation).

---

## WP4 — encryption (standard security handler)

Spec: 7.6.1 General + Table 20 (`Filter`, `V`, `Length`, `CF`, `StmF`,
`StrF`); 7.6.2 Algorithm 1 (per-object key, AES-CBC w/ 16-byte IV prefix,
PKCS#5 padding); 7.6.3.2 Table 21 (`R`, `O`, `U`, `P`, `EncryptMetadata`);
Algorithms 2–7; 7.6.5 crypt filters + Tables 25–26 (`CFM`: `None`/`V2`/
`AESV2`, `Identity`).

> **Two spec versions needed:** ISO 32000-1:2008 only defines **R 2/3/4,
> V 1/2/4** — RC4 (40–128 bit) and AES-128 (`AESV2`). **AES-256** (`V 5`,
> `R 6`, `AESV3`, `Length 256`, `U`/`O` 48 bytes, `UE`/`OE`/`Perms`
> entries) is specified in ISO 32000-2:2020 — offline under
> `offline/documentation/PDF/ISO_32000-2/`: 7.6.3.3 Algorithm 1.A (AES
> data encryption), 7.6.4.3.3 Algorithm 2.A (retrieving the file key),
> 7.6.4.3.4 Algorithm 2.B (hardened hash), 7.6.4.4.10/.11 Algorithms 11/12
> (password authentication), 7.6.4.4.12 Algorithm 13 (`Perms` validation).
> Our own fixture `odr-private/pdf/encrypted_fontfile3_opentype.pdf` is
> **R 6 / V 5 / AESV3**, so R 6 is in scope; pdf.js/qpdf serve as reference
> implementations. (R 5, the deprecated Adobe ExtensionLevel 3 interim
> revision with the weaker hash, is not worth supporting — files are rare
> and qpdf can rewrite them.)

### Design

New files `pdf_encryption.{hpp,cpp}`:

```cpp
class Decryptor {
public:
  // trailer_dict supplies /Encrypt (already resolved) and /ID.
  static std::optional<Decryptor> create(const Dictionary &encrypt,
                                         const std::string &file_id0);
  bool authenticate(const std::string &password); // tries user, then owner
  [[nodiscard]] bool authenticated() const;
  std::string decrypt_stream(const ObjectReference &, std::string data) const;
  std::string decrypt_string(const ObjectReference &, std::string data) const;
};
```

- **Supported configurations** (everything else → throw
  "unsupported encryption" with the `V`/`R`/`CFM` values in the message):
  - `V 1/2`, `R 2/3` — RC4, 40–128-bit key.
  - `V 4`, `R 4` — crypt filters; `StdCF` with `CFM` `V2` (RC4) or `AESV2`
    (AES-128-CBC); `Identity` passthrough; honour `StmF`/`StrF` (default
    `Identity` per Table 20); per-stream `/Crypt` filter `Name` override
    (Table 25/26); `EncryptMetadata false` → the extra `0xFFFFFFFF` salt in
    Algorithm 2 step (f).
  - `V 5`, `R 6` — AES-256-CBC, no per-object key derivation (file key used
    directly); key from password via the 2.B hash, unwrapped from `UE`/`OE`
    with AES-256-CBC, zero IV, no padding.
- **Key/password algorithms**: Algorithm 2 (32-byte pad constant, MD5, 50×
  re-hash for R ≥ 3), Algorithm 4/5 for `U` verification (R3+: compare
  first 16 bytes only), Algorithm 7 for owner password (RC4-decrypt `O`
  → user password, 20 iterations for R ≥ 3). Always try the **empty user
  password first** (most "protected" PDFs are owner-locked only — e.g. the
  Casio fixture below); only then surface `password_encrypted() == true`.
- **Per-object key** (Algorithm 1, V < 5): key + obj-id low 3 bytes +
  gen low 2 bytes (+ `sAlT` bytes for AES) → MD5 → first min(n+5, 16)
  bytes. AES data: first 16 bytes are the IV; strip PKCS#5 padding.
- **Crypto primitives**: Crypto++ is already a dependency (`crypto_util`).
  Add to `crypto::util`: `md5` (`Weak::MD5`), `rc4` (`Weak::ARC4`),
  `sha384`/`sha512` (for the 2.B hash; `sha256` exists), and a
  no-padding AES-CBC decrypt variant (existing `decrypt_aes_cbc` is
  padding-stripping; `UE`/`OE` unwrap needs raw). RC4/MD5 live in
  Crypto++'s `Weak` namespace — needs `CRYPTOPP_ENABLE_NAMESPACE_WEAK`;
  fine for decryption of legacy formats.
- **What gets decrypted** (7.6.1): all strings and streams, except the
  trailer `ID`, strings inside the Encrypt dict, xref streams (never
  encrypted), and strings inside object streams (the objstm payload is
  decrypted as one stream; its members are then plaintext — flag the
  source so the deep-walk skips them). Stream data is decrypted **before**
  filter decoding (7.6.2). Metadata streams honour `EncryptMetadata` via
  the `Identity` crypt filter mechanism.
- **Plumbing**: `DocumentParser` owns `std::optional<Decryptor>`.
  `parse_document` order: walk xref/trailer chain → if newest trailer has
  `Encrypt` → `read_object` it (its strings stay raw) → build Decryptor →
  authenticate("") → proceed. `read_object` deep-walks each freshly parsed
  object and decrypts `StandardString`/`HexString` leaves with the owning
  object's reference (skipping the Encrypt dict itself and objstm-sourced
  objects); `read_decoded_stream` decrypts before WP1 decoding.
- **Public API wiring** (`pdf_file.cpp`): `PdfFile` construction parses the
  trailer chain eagerly (it is cheap once WP2 exists) to answer
  `password_encrypted()` honestly; `decrypt(password)` runs
  `authenticate`, returns a decryptable `PdfFile` on success. Mirrors the
  CFB/ZIP `DecodedFile` decrypt contract. Permission bits (`P`, Table 22)
  are recorded but not enforced — read-only rendering, same stance as
  other readers.

### Tests

- `odr-public/pdf/Casio_WVA-M650-7AJF.pdf` — `R 2`, RC4-40, empty user
  password (verified present in fixture trailer).
- `odr-private/pdf/encrypted_fontfile3_opentype.pdf` — `R 6 / V 5 / AESV3`
  (verified); password documented in the test (or, if unknown, asserts the
  authenticate-failure path).
- Generate the missing matrix with `qpdf` (dev-time only): RC4-128 (R3),
  AES-128 (R4, incl. `EncryptMetadata false`), AES-256 (R6), each with
  user-password and owner-password-only variants → `odr-public/pdf/encrypted/`.
- The key/password algorithms are pure `string → string` functions, so they
  get inline known-answer tests with no file or parsing involved: password,
  `O`/`U`/`P`/`ID` inputs and the expected key as escaped-byte literals in
  the test source (Algorithms 2/4/5/7, plus the 2.B hash for R 6). Take
  vectors from a qpdf-encrypted file's dict (freeze them as literals) so
  the test is not circular through our own implementation. Same for
  Algorithm 1 per-object keys and one AES IV-prefix/padding round-trip.
  Only `authenticate`-against-real-file and full-document decryption need
  the fixtures above.

---

## WP5 — xref recovery (post-stage-0)

Not in the stage 0 exit criteria; design sketch so the WP2 code leaves room:

- Trigger: any structural throw during xref-chain walking or a failed
  object lookup (`startxref` missing/garbage, offsets wrong).
- Recovery: single forward scan for `n g obj` line starts (the existing
  sequential `read_entry` machinery is most of this), building a synthetic
  `Xref` (last definition of an id wins), collecting `trailer` dicts and
  `/Type /Catalog` objects as `Root` candidates; objstm members indexed by
  scanning recovered object streams.
- Recovery tests are a perfect fit for inline strings: since the scan
  ignores xref offsets entirely, a broken mini-PDF needs no offset
  bookkeeping — write the literal by hand with a garbage `startxref`,
  duplicate object ids, or a missing trailer, and assert what got
  rebuilt. Real-world fixture: `odr-private/pdf/order-EK52VKL0.pdf` — an
  HTTP response accidentally saved as `.pdf` (starts with
  `HTTP/1.0 200 OK`, the PDF body follows) — exactly the class of file
  this rescues.

---

## Engineering notes (all WPs)

- All new diagnostics go through `Logger` (cross-cutting item in
  `PLAN.md`), not stdout/stderr.
- Positions stay `std::uint32_t` (≥ 4 GiB files remain out of scope).
- `Xref::Entry` change touches `read_xref`, `Xref::append`, and the two
  existing smoke tests — convert those to assertions while there.
- The offset-computing mini-PDF builder is test-only code; put it next to
  the tests (e.g. `test/src/internal/pdf/pdf_test_file_builder.{hpp,cpp}`),
  not in `src/` — it must never grow into a writer API.
- Keep `STATUS.md` updated as WPs land (xref-stream gap, filter gap, and
  encryption stubs are all called out there today).
