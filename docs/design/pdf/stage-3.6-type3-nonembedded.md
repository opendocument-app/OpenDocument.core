# PDF stage 3.6 — Type3 glyphs + non-embedded fonts

Design for the final stage-3 sub-stage. Status: **design draft** (no
implementation yet — this PR seeds the branch). Roadmap entry lives in
[`src/odr/internal/pdf/AGENTS.md`](../../../src/odr/internal/pdf/AGENTS.md).

Two loose ends that don't fit the "read a font program, wrap to OTF" pipeline:
fonts whose glyphs are **drawing procedures** (Type3) and fonts with **no
embedded program at all** (the standard 14 + common substitutes). Closes stage
2's deferred AFM-widths item.

## Part A — Type3 fonts

Type3 glyphs are mini content streams, not outlines, so they cannot go through
`@font-face`. They need path → SVG, which otherwise belongs to stage 4; a
**minimal path → SVG capability is pulled forward here** (decision 2026-06-19)
rather than waiting on the full graphics stage.

### What gets read

A Type3 font dictionary carries:
- **`/CharProcs`** — glyph name → content stream (drawing procedure).
- **`/Encoding`** — code → glyph name (Differences).
- **`/FontMatrix`** — glyph space → text space (Type3 fonts set their own, not
  the implicit 1/1000).
- **`/FontBBox`**, **`/Resources`** — resources referenced by the procs.

### Rendering

Each char proc is already tokenized by the operator parser. The proc begins with
`d0` (colored) or `d1` (uncolored — glyph takes the fill color, bbox follows).
Run it through a small **path → SVG** emitter covering the path-construction +
painting subset (`m`/`l`/`c`/`v`/`y`/`re`/`h`, `f`/`F`/`f*`/`S`/`s`/`B`/`b`,
`W`/`W*` clip, `cm`, color ops) — the minimal slice of stage 4. Each glyph
becomes an SVG `<symbol>`/`<g>` in glyph space; the HTML layer instantiates it
per show at the text transform (CTM × Tm × FontMatrix, font size folded in),
sized by `/FontMatrix`.

Type3 has no glyph program → no PUA re-encode, no reverse map. Unicode for
selection still comes from the stage-1 chain (`/ToUnicode` / `/Encoding`); the
dual-layer model holds (SVG glyph layer + transparent Unicode layer).

This path → SVG emitter is written to be **reused by stage 4** for page-level
vector content.

## Part B — non-embedded fonts

A font with no `/FontFile*`: substitute and metric-match rather than render true
glyphs.

- **Substitution** — map the standard 14 (Helvetica/Times/Courier families +
  Symbol/ZapfDingbats) and common names to CSS `font-family` fallback stacks
  (serif/sans/mono by flags + name heuristics; bold/italic from the descriptor
  `/Flags` and the name). No `@font-face`.
- **Metrics** — drive placement from the PDF `/Widths` when present; for the
  standard 14 (which usually ship no `/Widths`) use the **AFM advance-width
  tables**, closing stage 2's deferred item. AFM widths become a generated data
  table (`tools/pdf/generate_*`), like the encoding / AGL tables in
  `pdf_encoding_data`.
- Glyph shapes are the browser's fallback font — display fidelity is bounded
  here by design (no program to embed); selection/search are exact.

## Module touchpoints

- `internal/svg/` (or `internal/font/type3_*`) — the minimal path → SVG emitter
  (new, shared with stage 4).
- `html/pdf_file.cpp` — Type3 SVG glyph emission; non-embedded substitution +
  font-family stacks.
- `pdf_encoding_data` (or sibling) — generated AFM width tables for the standard 14.
- `pdf_document_element` — Type3 `/CharProcs`/`/FontMatrix`/`/Resources` on
  `Font`; substitution facts for the non-embedded path.

## Scope / non-goals

- Only the path-painting operator subset needed by real Type3 procs; full stage-4
  graphics (shadings, images, patterns) stays in stage 4.
- No font synthesis for non-embedded fonts — substitution + metrics only.

## Tests

Type3: an inline content-stream font (a `d1` proc drawing a rectangle) asserting
the emitted SVG path and its placement transform. Non-embedded: standard-14 AFM
width lookup through `advance_width`, and the substitution family-stack mapping
for a few representative names/flag combinations.
