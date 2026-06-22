# Stage 3.3 — wire TrueType into PDF `@font-face`

First end-to-end PDF display win. Read embedded TrueType programs
(`FontFile2` / `CIDFontType2`) through `abstract::Font`, run them through the
3.1 PUA/OTF pipeline, and emit glyph-exact spans in the PDF HTML — replacing
today's fallback-font span. Also lands the **embedded-font reverse map**
(code → Unicode from the reversed `cmap`), closing the stage-1 extraction gap
for these fonts.

Scope: **TrueType only** (`/FontFile2`). Bare CFF (`/FontFile3`) is stage 3.4,
Type1 (`/FontFile`) stage 3.5; those descriptors leave `Font::program` null and
the font keeps rendering in the fallback path.

## Decisions

- **Dual layer for text (chosen 2026-06-22).** Each shown segment with an
  embedded program emits two overlaid absolutely-positioned spans: a **visible
  glyph layer** (PUA code points in the embedded `@font-face` font, glyph-exact)
  and, when Unicode is recoverable, a **transparent selectable layer** carrying
  the real Unicode for native copy/search. The glyph layer is `aria-hidden` +
  `user-select:none`; the text layer is `color:transparent`. Trade-off
  acknowledged: up to ~2× text spans (the DOM-size concern the HTML layer already
  flags) — revisit if a corpus file makes it bite.
- **Uniform PUA (decision 2026-06-19).** Every embedded font is re-encoded to
  `U+E000 + glyph` and served via `@font-face`; the glyph layer's text is the PUA
  code point per shown glyph. No mapped-vs-unmapped branch.
- **Reverse map flows through `Font::to_unicode`.** Implemented as the new final
  fallback in `Font::to_unicode` (code → glyph → `code_point_for_glyph`), so
  `extract_text` picks it up with no page-text changes: previously-`no_unicode`
  runs become selectable automatically, and the dual layer's text layer is
  populated for free.

## Code → glyph resolution (`Font::glyph_for_code`)

The visible glyph layer needs a glyph id per character code; the reverse map
needs the same. `Font::glyph_for_code(code)`:

- **Composite (Type0) CIDFontType2** — the dominant modern case. `Identity-H/V`:
  CID = code. CID → GID via `/CIDToGIDMap`: `Identity` (default) → GID = CID;
  an explicit stream → `GID = cid_to_gid[CID]`.
- **Simple TrueType** — best effort per ISO 32000-1 9.6.6.4: try the embedded
  `cmap` directly on the byte code (covers symbolic `(3,0)`/`(1,0)`), else via the
  font's own `glyph_for_code_point` keyed on the code's Unicode (from the
  `/Encoding` chain), else treat the code as a GID. Refinements deferred.

## Touch points

- `pdf_document_element.hpp` / `pdf_document.cpp`: `Font::program`
  (`shared_ptr<abstract::Font>`), `Font::cid_to_gid`, `Font::glyph_for_code`,
  reverse-map fallback in `Font::to_unicode`.
- `pdf_document_parser.cpp`: load `/FontFile2` (decoded stream → `SfntFont`) from
  the font (composite: the descendant CIDFont's) `/FontDescriptor`; read
  `/CIDToGIDMap`. A parse failure logs and leaves `program` null.
- `html/pdf_file.cpp`: per-font `@font-face` (re-encode once, data URL), dual-layer
  span emission, keep the fallback path for fonts without a program.

## Deferred

- Non-identity simple-TrueType glyph selection edge cases; CFF/Type1 (3.4/3.5).
- Baseline placement (font ascent metrics), still box-top.
- The ~2× span cost mitigation, if it bites.
