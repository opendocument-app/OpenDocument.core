# PDF stage 3.4 — bare CFF (`/FontFile3` / Type1C)

Design for the next font sub-stage. Status: **design draft** (no implementation
yet — this PR seeds the branch). Roadmap entry lives in
[`src/odr/internal/pdf/AGENTS.md`](../../../src/odr/internal/pdf/AGENTS.md);
this is the detailed design that precedes implementation.

## Goal

Read a bare **CFF** font program — a PDF `/FontFile3` with `/Subtype /Type1C`
(simple) or `/CIDFontType0C` (composite), and OpenType-CFF (`OTTO`) — into an
`abstract::Font`, wrap it into a browser-loadable OTF, and wire it into PDF
output exactly the way TrueType was wired in 3.3. After this stage an embedded
CFF font renders its real glyphs through `@font-face` and recovers Unicode for
selection via the reverse map.

Reading CFF (charset, charstrings, private dict) is the easy part of the font
work and also yields the **CFF embedded-font reverse map** (glyph → name → AGL →
Unicode), closing one more case of the stage-1 extraction gap (the TrueType case
landed in 3.3; Type1 follows in 3.5).

## What gets read (`internal/font/cff_font.{hpp,cpp}`)

A new `cff::CffFont : abstract::Font`, mirroring `sfnt::SfntFont`'s shape (reads
from a `std::istream`, raw bytes kept for pass-through `data()`). CFF structure
parsed per Adobe TN #5176:

- **Header**, **Name INDEX**, **Top DICT INDEX**, **String INDEX**, **Global
  Subr INDEX** — the INDEX/DICT primitives are the bulk of the reader.
- **Charset** — glyph id → SID (or CID for CID-keyed fonts). Source of the
  reverse map for simple fonts.
- **CharStrings INDEX** — `glyph_count()` is its count; charstrings pass through
  byte-for-byte (no outline interpretation).
- **Private DICT** (+ local Subrs) — `defaultWidthX` / `nominalWidthX` for
  advance widths.
- **Encoding** — built-in code → glyph (Standard/Expert/custom); only relevant
  for a simple font with no PDF `/Encoding` override.
- **CID-keyed extras** — `ROS`, `FDArray`, `FDSelect` when the Top DICT carries
  `ROS`; charset then maps glyph → CID.

### `abstract::Font` facts

| Fact | Source |
|------|--------|
| `format()` | `FontFormat::cff` (new enum value if absent) |
| `name()` | Name INDEX entry 0 |
| `glyph_count()` | CharStrings INDEX count |
| `units_per_em()` | `1000 / FontMatrix[0]` (Top DICT `FontMatrix`, default 1000 upm) |
| `advance_width(glyph)` | leading width operand of the charstring (cheap: read only the optional width that precedes the first stack-clearing op) else `defaultWidthX` — **not** a full charstring interpretation |
| `glyph_for_code_point` / `code_point_for_glyph` | charset glyph ↔ SID → glyph name → AGL ↔ Unicode (reuse `pdf_encoding`'s AGL) |

PDF-side advance metrics still come from `/Widths` / `/W` as before; the
`advance_width` here only feeds the OTF `hmtx` skeleton.

## OTF wrap (`internal/font/cff_transform` or extend `sfnt_transform`)

A bare CFF has none of the SFNT metric tables. Synthesize the required skeleton
from the `abstract::Font` facts and embed the CFF verbatim as the `CFF ` table,
producing an `OTTO`-version SFNT:

- Synthesize `head`, `hhea`, `hmtx`, `maxp` (v0.5 for CFF), `cmap`, `name`,
  plus `post` / `OS/2` (reuse `serialize_post` / `serialize_os2`).
- `cmap` is the **uniform PUA map** — reuse the 3.1 pipeline. `reencode_to_pua`
  currently takes `sfnt::SfntFont&`; generalize it (or the wrap) to assign
  `pua_code_point(glyph)` for every glyph over the synthesized `cmap` so a CFF
  wraps through the same path.
- Assemble via `build_sfnt(out, 'OTTO', tables)` — checksum handling already
  analytic.

OTS over every produced font remains the CI gate (test-only).

## PDF wiring (reuse 3.3)

- `pdf_document_parser`: when a font descriptor carries `/FontFile3`, decode it
  and build a `CffFont` into `Font::embedded_font` (today only `/FontFile2` →
  `SfntFont` is loaded). `/Subtype` distinguishes Type1C / CIDFontType0C /
  OpenType.
- `Font::glyph_for_code` gains a CFF branch: simple CFF → built-in/`/Encoding`
  charset lookup; `CIDFontType0C` → CID → GID via the CID-keyed charset (or
  identity when charset is `Identity`).
- `Font::to_unicode` reverse-map fallback already calls `code_point_for_glyph`
  — works unchanged once `embedded_font` is a `CffFont`.
- HTML dual-layer emission (glyph layer + transparent Unicode layer) is
  format-agnostic and needs no change.

## Scope / non-goals

- No charstring outline interpretation (pass-through; widths read cheaply).
- Type1 (`/FontFile`) is 3.5; Type3 / non-embedded is 3.6.
- format-12 / multi-plane PUA spill-over inherits 3.1's BMP-only limitation.

## Tests

Font-only, assertion-based, no shipped fixtures — mirror
`pdf_font_program.cpp` / the sfnt tests: a tiny hand-built CFF (or a minimal
real Type1C frozen as a literal) exercising INDEX/DICT parsing, charset reverse
map, the synthesized-skeleton round-trip through OTS, and the PUA re-encode.
Plus a `pdf_document_parser` case: a `/FontFile3` font reaching
`embedded_font` and a glyph/Unicode recovery assertion.
