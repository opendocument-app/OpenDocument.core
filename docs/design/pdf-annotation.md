# PDF annotation design

Status: **underway.** This records the architecture for adding markup
annotations — text highlight and freehand drawing first — to an existing PDF,
the alternatives weighed, and the effort it costs. The format model is
validated against four viewers, and Phases 0 through 3 have landed: the writer
appends, and the markup and ink annotations it carries are written.

Scope is **markup only**: draw on top of a page, highlight/underline/strike
text. Editing or removing the *existing* text of a PDF is explicitly out — that
is a different feature with a different cost.

Related: [`editing.md`](editing.md) for ODF/OOXML content editing, which this is
independent of (decision 1), and
[`pdf/AGENTS.md`](../../src/odr/internal/pdf/AGENTS.md) for the read side this
builds on.

## Problem

`pdf/` is read-only: it parses and renders to HTML, and has no writer. We want
the user to highlight text and draw on a page in the browser, and to persist
that into the PDF as **standard annotations**, so every other viewer sees them —
not as a rasterized overlay and not as a sidecar file.

## Why this is cheap here

Annotations are **additive**. Nothing in the page content stream changes, no
object is renumbered, no page-tree meaning is touched. A highlight is one new
annotation object, one new appearance-stream object, and a rewritten page
dictionary — appended to the file as an incremental update (7.5.6).

Most of the machinery is already in the tree:

| Piece | Where | State |
|---|---|---|
| Object serialization | `pdf_object.cpp` `to_stream`/`operator<<` | Emits real PDF syntax, not a debug dump. Gaps: `StandardString` has a `// TODO escape`, `Name` does not `#`-escape, reals must not reach exponent form |
| Whole-file assembly with xref + `startxref` | `test/.../pdf_test_file_builder.cpp` | Works, but is marked *"must never grow into a writer API"* — `src/` gets its own |
| Original bytes | `PdfFile::m_file` | An incremental update is copy-then-append |
| Page geometry | `begin_page()` → `to_box` (`html/pdf_file.cpp`) | User space → page box in points, `/CropBox` origin and `/Rotate` folded in; needs `Transform2D::inverse` |
| **Appearance-stream rendering** | `Annotation::appearance`, `extract_annotation` | `/AP /N` already resolves (through `/AS`), fits `/Matrix`-transformed `/BBox` onto `/Rect`, and runs like a `Do` |
| **Blend modes** | `blend_mode_to_css` | `/BM /Multiply` — what a highlight needs — already maps to `mix-blend-mode` |
| Selectable text with geometry | the `.sel` dual layer | `Range.getClientRects()` yields highlight quads directly |
| Embedded script/style assets | `frontend.cpp` (`viewport_js`, `search_js`) | A new `pdf_annotation_js` slots in unchanged |

The appearance rendering is the load-bearing one: **what we write, we already
render**, so the round-trip is self-verifying and the feature needs no new
rendering code.

## Decisions

### 1. File-level API on `PdfFile`, addressed by page + geometry

PDF has no `abstract::Document` (`is_decodable()` is `false`; there is only an
`HtmlService`), so `Document::edit`/`save` do not apply. The entry point hangs
off `PdfFile` and addresses annotations by **page index and user-space
geometry**, never by `ElementIdentifier`.

**Why it matters:** this sidesteps the id-stability linchpin that
[`editing.md`](editing.md) Phase 0 is blocked on. No append-only/tombstone
discipline, no session-stable ids, no JS/C++ op-semantics drift, so no
conformance corpus. **This feature does not depend on the deferred editing work
and must not be sequenced behind it.**

### 2. Incremental update, never a rewrite

Append a new section to the original bytes; never re-serialize the document.

**Why:** a full rewrite means re-emitting every object we parsed, which turns
every read-side gap (an unmodelled key, a filter we pass through, an object
stream we did not recompress) into data loss. An incremental update copies the
original byte-for-byte and is the mechanism the format provides for exactly
this. It also keeps an existing signature valid over its own byte range — the
file is flagged *modified after signing*, not broken, which is what Acrobat does
too.

**Consequence:** files we could only open by the forward-scan xref rebuild must
be **refused**, not annotated — appending onto a structure whose own xref is
broken produces a file that only we can read.

### 3. Always write an appearance stream

Every annotation carries `/AP /N` — a form XObject we generate — even where a
viewer could synthesize one from `/QuadPoints` or `/InkList`.

**Why:** our own renderer paints annotations *only* through `/AP /N`
(`pdf/AGENTS.md`), so without one the annotation is invisible in odr. Writing it
is also the interop-safe choice: viewers disagree on synthesized appearances,
and none disagrees about a form XObject. The constraint and the correct answer
coincide.

### 4. Hand-roll the writer; take no PDF library

| Option | License | Verdict |
|---|---|---|
| MuPDF (`pdf_annot`, exactly this feature set) | AGPL / commercial | Incompatible with MPL-2.0 |
| PDFium (`FPDFAnnot_*`, `FPDF_INCREMENTAL`) | BSD-3 | License fine; it is a full renderer + parser, and pulling that in to append four dictionaries contradicts the module's premise |
| PoDoFo | LGPL | Static linking on iOS is a licensing problem; second object model beside ours |
| QPDF | Apache-2.0 | Clean, good object model — but a second PDF parser next to the one we wrote. Worth revisiting only for a *general* writer |
| pdf-lib / pdfAnnotate (JS) | MIT | No C++ work, but duplicates file writing in JS, does not serve the droid/ios native path, and adds a JS dependency the project avoids |

The part that normally makes PDF writing expensive — building the object graph,
embedding fonts, emitting content streams — we either already have or do not
need. The genuinely new logic is a few hundred lines.

### 5. Fat browser, same as `editing.md`

The browser owns the pending annotations for the session and renders them live;
on save it hands C++ a JSON list which is applied in one shot. C++ re-validates
and fails fast.

**Why:** identical reasoning to [`editing.md`](editing.md) decision 1, and here
the drift risk that decision accepted does not exist — the payload is
declarative geometry, not an operation log with semantics to reimplement on both
sides.

### 6. Refuse encrypted files in v1

New strings and streams must be encrypted with the file key, and the module
deliberately never retains the derived key (it lives inside `Decryptor`, with no
accessor). `crypto::util` has `encrypt_aes_cbc` and RC4 is symmetric, so this is
reachable later — PKCS#5 padding, a random IV, and a key accessor — but v1
throws rather than writing a file whose new objects are in the clear and
therefore unreadable.

## Wire format

One JSON document, produced by the browser, consumed by `PdfFile::annotate`.
Coordinates are **PDF user space** (points, y-up, the page's own space) — the
browser has already applied `to_box⁻¹`, so C++ does no geometry beyond building
the appearance.

```jsonc
{
  "version": 1,
  "annotations": [
    {
      "page": 0,                       // 0-based index into the page tree
      "type": "highlight",             // highlight | underline | strikeOut | squiggly
      "quads": [                       // one per line covered; user space
        [72.0, 700.0, 300.0, 700.0,    //   x1 y1  x2 y2  (upper-left, upper-right)
         72.0, 688.0, 300.0, 688.0],   //   x3 y3  x4 y4  (lower-left, lower-right)
        [72.0, 686.0, 180.0, 686.0, 72.0, 674.0, 180.0, 674.0]
      ],
      "color": [1.0, 0.9, 0.2],        // DeviceRGB, 0..1
      "opacity": 1.0,                  // /CA
      "author": "…",                   // /T, optional
      "contents": "…"                  // /Contents, optional
    },
    {
      "page": 0,
      "type": "ink",
      "strokes": [                      // one entry per pen-down..pen-up
        [100.0, 500.0, 104.5, 502.0, 110.0, 507.5]   // flat x y pairs
      ],
      "color": [0.9, 0.1, 0.1],
      "width": 2.0,                     // /BS /W, points
      "opacity": 1.0
    },
    { "page": 1, "type": "delete", "name": "odr-3f2a91c4" }   // /NM of one we wrote
  ]
}
```

Notes on the shape:

- **`quads` order is upper-left, upper-right, lower-left, lower-right.** The
  spec's stated order (12.5.6.10) is counterclockwise; every implementation
  writes the Z-order above, and `pdfAnnotate`'s documentation says as much
  outright, as does Phase 2's appearance-less experiment. Follow the
  implementations, and say so in a comment at the one place that emits it.
- **`delete` only names an annotation we wrote**, identified by the `/NM` we
  minted. Deleting a foreign annotation is out of scope: we would have to prove
  nothing else references it.
- The payload is one-way and non-invertible; undo lives in the browser, exactly
  as [`editing.md`](editing.md) decision 6 argues.
- `version` is the drift guard — a payload from a newer frontend is rejected,
  not partially understood.

## What gets written

For a highlight, three objects and one rewrite:

```
<original bytes, unchanged>
12 0 obj << /Type /Annot /Subtype /Highlight /Rect [72 674 300 700]
            /QuadPoints [72 700 300 700 72 688 300 688  …]
            /C [1 0.9 0.2] /CA 1 /F 4 /NM (odr-3f2a91c4) /M (D:20260906120000Z)
            /AP << /N 13 0 R >> >> endobj
13 0 obj << /Type /XObject /Subtype /Form /BBox [72 674 300 700]
            /Group << /Type /Group /S /Transparency /CS /DeviceRGB >>
            /Resources << /ExtGState << /G0 << /BM /Multiply /ca 1 >> >> >>
            /Length n >> stream
              /G0 gs 1 0.9 0.2 rg 72 688 228 12 re f  …
            endstream endobj
5 0 obj  << … original page dictionary …, /Annots [9 0 R 12 0 R] >> endobj
xref  (only the changed ids)
trailer << /Size … /Root … /Prev <previous startxref> /ID [<first> <new>] >>
startxref …
```

Ink is the same shape: `/Subtype /Ink`, `/InkList [[x y …]]`, `/BS << /W w >>`,
and an appearance of `m`/`c`/`S` with round caps and joins.

The transparency group on the form is what makes `/BM /Multiply` composite
against the page rather than against the form's own backdrop.

### Validated against real viewers

A throwaway script wrote exactly the above — a highlight and an ink stroke, as
one incremental update — onto `odr-public/pdf/style-various-1.pdf`, before any
of it was committed to C++. `qpdf --check` passes and four independent engines
paint both annotations with the page text showing through the highlight:
ghostscript, PDFium (Chrome), CoreGraphics (Preview), and **our own renderer**,
which emits the highlight as `<path fill="rgb(255,230,51)"
style="mix-blend-mode:multiply">` and the ink as a round-capped stroke.

So the following are facts, not assumptions: the transparency group composites
against the page rather than a black backdrop; appending to a page's *existing*
`/Annots` array works and the newer page object wins; a classic section listing
only the changed ids is accepted everywhere; and `to_box` places the result
correctly (user-space y 700/688 arrived at page-box y 92/104).

Three things the spike did **not** settle, and Phase 1 and 2 owe tests for each:

- **QuadPoints ordering.** With an `/AP` present, the appearance is what every
  one of those engines painted — the `/QuadPoints` were never consulted.
  Settled in Phase 2 with an appearance-less annotation instead.
- **A page dictionary inside an object stream**, and **appending to a file
  whose newest section is an xref stream.** The spike's fixture had neither;
  Phase 1's tests cover both.

## Implementation plan

Ordered so each step is verifiable on its own. Estimates are working days.

### Phase 0 — serialization correctness — **done** (#843)

`Transform2D::inverse`; escaping for `StandardString`, `Name` and dictionary
keys; reals through `util::number::to_string_significant`, since `{:.4g}` both
rounded to four significant digits and reached for an exponent form 7.3.3 has
no syntax for. Pinned by a round trip through `ObjectParser`.

### Phase 0.5 — the parse facts a writer needs — **done** (#844)

Appending needs four things about the file, and `DocumentParser` computed all
four while keeping one. `xref()`/`trailer()` were reachable; the newest
section's offset, its kind, and the recovery flag were not. Now
`start_xref_position()`, `xref_kind()`, `is_recovered()` and
`highest_object_id()`.

The first two are `std::optional` and recovery clears them — a rebuilt table
has no section of the file's own to chain onto, so the missing value and
decision 2's refusal gate are the same fact.

### Phase 1 — the incremental writer — **done** (#845)

`pdf/pdf_writer.{hpp,cpp}`: `IncrementalWriter` pipes the source through
untouched, appends the objects it collected, and closes with a cross-reference
section naming only their ids and a trailer chaining back through `/Prev`.

- **Matches the file's xref flavor** (`xref_kind()`), classic table or
  cross-reference stream — the latter minting an id and an entry for the stream
  object itself.
- **Refuses** a recovered file (decision 2) and an encrypted one (decision 6),
  resolving both gates in the constructor so nothing downstream re-asks.
- **`/ID[1]` is derived from the update's own bytes**, not from a clock, so
  writing the same update twice gives the same file and a test can pin it.
- Only the update is buffered; the source is piped, so appending to a large
  file does not hold it in memory.

Verified in the order the plan asked for — a no-op update that re-parses
identically first, then a `/Rotate` rewrite. `qpdf --check` passes, and
ghostscript, CoreGraphics and our own renderer all honour the new rotation
(the page box turns 8.5×11in into 11×8.5in).

A page dictionary living inside an object stream is rewritten uncompressed in
the new section, the newer type-1 entry winning over the older type-2 one.

### Phases 2 and 3 — text markup and ink — **done** (#847)

`pdf/pdf_annotation.{hpp,cpp}`: `write_text_markup` covers `/Highlight`,
`/Underline`, `/StrikeOut` and `/Squiggly`; `write_ink` covers `/Ink`, its
strokes smoothed Catmull-Rom → cubic bezier. `append_page_annotations` puts
them on the page, rewriting the `/Annots` array itself where it is indirect.

Only the highlight multiplies (11.6.4.1) — it is a wash over the text, where
the others are marks drawn on top of it.

**`/QuadPoints` ordering is settled.** Two files carrying the same visual
rectangle, one in Z-order and one in the spec's counterclockwise order, each
with no `/AP` so a viewer has to synthesize the appearance: ghostscript draws
the Z-order as a clean rectangle and the spec's order as a twisted, smeared
blob. CoreGraphics synthesizes nothing at all, so it is no oracle here.

### Phase 4 — public API (1 d, ~130 lines)

`PdfFile::annotate(std::string_view json, std::ostream &out, const Logger &)`,
throwing per the repo's fail-fast rule. A `FileTypeCapabilities` bit for it, and
the `file_type_table` row (the capability test fails if the declaration exceeds
what the engine does).

### Phase 5 — browser layer (4–6 d, ~700 JS + 80 CSS)

`pdf_annotation_js` in `frontend.cpp`, following `viewport_js`/`search_js`:

- Per-page overlay SVG, live preview of pending annotations.
- Highlight tool: selection → `getClientRects()` → merge per line, drop the
  zero-width spacer spans of the `.sel` layer, clip to the page box.
- Ink tool: pointer events, coalesced points.
- Coordinate helper: client rect → page-div rect → scale by
  `divRect.width / pageWidthPt` (robust against the zoom script's CSS transform)
  → `to_box⁻¹`.
- Undo/redo, colour, delete-by-hit-test.
- `odr.getAnnotations()` returning the payload above.

### Phase 6 — bindings (2 d, ~470 lines)

wasm (~50 C++, ~80 TS), JNI (~60 C++, ~70 Java), Python (~40), Apple (~80 ObjC,
~90 Swift).

### Phase 7 — corpus and interop (2 d, ~600 test lines)

Reference-output snapshot entries; interop check of our output in Acrobat,
Preview and pdf.js.

**Total ≈ 15–20 days, ≈2,700–3,300 lines** — about 1,000 C++ in `src/`, 600 C++
test, 780 JS/CSS, 470 bindings. Per-app UI (droid/ios toolbars) is on top and
outside this repo.

**Narrower MVP** — highlight and ink, wasm only, unencrypted, no delete —
**6–8 days, ~1,400 lines**, and shippable, because the render side already
exists.

## What the writer unlocks next

Nearly free once Phases 0–2 land, all reusing the same appearance machinery:

- **Underline / StrikeOut / Squiggly** — the highlight path with a different
  appearance and subtype.
- **Square / Circle / Line** — the ink path.
- **Sticky note** (`/Text` + `/Contents`) — trivial in the file; the cost is the
  popup UI.
- **Page rotate** — rewrite `/Rotate` on the page dictionary.
- **Page delete / reorder** — rewrite `/Kids` and `/Count`.
- **`/Info` metadata edit** — one new dictionary.

Medium:

- **FreeText** — needs `/DA` and a font resource; generate the appearance with a
  standard-14 Helvetica, which the substitution path already handles.
- **Stamp / signature image** — an image XObject in the appearance; `png/`
  already encodes.
- **Flatten annotations** — append to the content stream.
- **AcroForm field fill** — the writer makes it possible, but regenerating
  appearances from `/V` and `/DA` is the real work, and `pdf/AGENTS.md` scopes
  form interactivity out today.

## Open questions

- **Link overlays vs. the highlight tool.** `<a>` overlays already sit above the
  `.sel` layer and block selection (`pdf/AGENTS.md` roadmap). A
  selection-driven highlight tool makes that conflict user-visible rather than
  theoretical — does this feature force the reverted `elementFromPoint`
  workaround (commit `5cfa8a09`) back onto the table?
- **Annotating a linearized file** breaks its linearization: the `/Linearized`
  dictionary then describes a prefix that is no longer the whole file. Viewers
  cope and Acrobat does the same — do we say so and move on, or de-linearize?
- **Encrypted files**: is refusing acceptable for the app's real corpus, or does
  the `Decryptor` key accessor need to land in v1 after all?
- **Where does the pending-annotation state live across a reload** in the mobile
  WebView — the browser only, or does the host persist the payload?
