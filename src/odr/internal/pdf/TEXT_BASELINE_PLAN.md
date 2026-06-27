# PDF→HTML — text baseline placement plan

> **Status: implemented** in this branch. Each run is now placed by its
> baseline using `ascent_em` (FontDescriptor `/Ascent` → embedded-font bounding
> box → 0.8 em) with `line-height:1` pinning the leading. The remaining
> approximation (the leading is exact only when ascent + descent ≈ 1 em, and the
> ascent source is per-font, not per-CID) and the open questions below are left
> for follow-up. This file is kept as the design record.

Standalone follow-up, independent of the stage-4 graphics plan
(`STAGE4_PLAN.md`). Fixes the text vertical-offset bug formerly noted in two
places (the `TODO baseline sits at the box top` comment in
`src/odr/internal/html/pdf_file.cpp` and the deferral note in
`src/odr/internal/pdf/AGENTS.md`): the text origin sat at the CSS box top, so
every run rendered ~one ascent below its baseline.

## The bug

In PDF, the text-space origin is the glyph **baseline**. The HTML emitter places
each run with the origin as the CSS `top` (`pdf_file.cpp:580-581`):

```cpp
add_class(base, "l", px_decl("left", round2(m.e * pt_to_px)));
add_class(base, "t", px_decl("top",  round2(m.f * pt_to_px)));   // m.f == baseline y
```

CSS `top` anchors the **top edge** of the line box, not the baseline, so every
run is pushed down by roughly one ascent (≈ the font height). Vector graphics
(highlights, rules, fills) are painted from PDF user space via `to_box` and land
correctly, so visually the text sits ~one ascent **below** its highlight box.
The text is what is mis-placed; the drawings are correct.

## Why this is not a one-liner

CSS does not let you say "put the baseline here" directly. The baseline's
position inside an inline box is `top + half_leading + ascent`, where `ascent`
is taken from the **rendering font's own** `hhea`/`OS/2` metrics (as the browser
reads them) and `half_leading` comes from `line-height`. So a correct fix needs:

1. A known ascent value, ideally the *same* value the browser will use.
2. Control of leading so `half_leading` is a known constant (zero).

We are in a good position for (1): for embedded fonts we synthesize the
`OS/2`/`hhea` tables ourselves on re-encode (`font/cff_transform.cpp`
`serialize_hhea`/`serialize_os2`; the SFNT path passes the originals through),
so the metric we compute can be made to match the metric the browser sees.

## Available metrics

`abstract::Font` already exposes (see `src/odr/internal/abstract/font.hpp`):

- `units_per_em()` — design units per em (1000 / 2048).
- `bounding_box()` → `FontBBox{y_min, y_max, …}` — design-unit glyph extents.

Not yet captured: the PDF FontDescriptor `/Ascent` (and `/Descent`,
`/FontBBox`). `pdf::Font` (`pdf_document_element.hpp:192`) holds `embedded_font`
but no descriptor metrics.

**Ascent source priority** (express everything as a fraction of the em,
`ascent_em`):

1. PDF FontDescriptor `/Ascent` ÷ 1000 — authoritative for the PDF's intent.
   Requires plumbing the descriptor into `pdf::Font` (new field, set in
   `pdf_document_parser.cpp` where the descriptor is already read).
2. Embedded font `OS/2.sTypoAscender` (or `hhea.ascender`) ÷ `units_per_em()`.
   Closest to what the browser renders. Needs a small accessor on
   `abstract::Font`/`SfntFont`, or reuse `bounding_box().y_max / units_per_em()`
   as a coarser stand-in.
3. Constant fallback `0.8` em for the legacy / no-embedded-font path, matching
   the `serialize_os2` degenerate fallback (0.8/0.2 em) so the fallback font and
   our math agree.

## Design

Anchor by baseline: subtract the ascent (in CSS px) from `top`, and pin
`line-height` so there is no half-leading to account for.

### Placement math

`ascent_px = ascent_em * font_size_px`, where `font_size_px` is the CSS pixel
size already computed for the run.

- **Uniform branch** (`pdf_file.cpp:576-584`): the CSS font size is
  `m.a * text.size * pt_to_px`, so

  ```
  top = (m.f - ascent_em * m.a * text.size) * pt_to_px
  ```

  i.e. keep `m.e` for `left`, replace the `top` declaration with the
  ascent-shifted value.

- **Matrix branch** (`pdf_file.cpp:585-593`): the CSS font size is
  `text.size * pt_to_px` and the matrix carries the scale. The ascent shift is
  along the text-space y axis, so it must go *through* the matrix, not be
  subtracted from `m.f` afterwards. Pre-translate the run by `-ascent_em` em in
  text space before composing, i.e. fold a `translation(0, -ascent_em * size)`
  (text-space units) into `m` ahead of `flip_glyph * text.transform * to_box`,
  then emit the matrix as today. (Equivalently, adjust the `e`/`f` of the final
  matrix by the transformed ascent vector.)

### Leading

Add `line-height:1` to the shared `.t` rule (`pdf_file.cpp:805-807`) and to the
`placement` constant for the nested glyph layer (`pdf_file.cpp:751-753`) so the
browser adds no extra half-leading above the ascent. Verify that `1` (vs
`normal`) does not itself reintroduce an offset with the embedded metrics;
`line-height:1em` over a single-line `white-space:pre` span should give a
content box whose top-to-baseline distance is exactly the font ascent.

## Plumbing summary

1. `abstract::Font` (+ `SfntFont`, `CffFont` impls): add an `ascent()` accessor
   in design units (or `ascent_em()`), reading `OS/2`/`hhea`, falling back to
   `bounding_box().y_max`.
2. `pdf::Font`: add `descriptor_ascent` (optional, /1000) populated in
   `pdf_document_parser.cpp`.
3. `pdf_file.cpp`: a helper `ascent_em(const pdf::TextElement&)` applying the
   source priority above; use it in both placement branches and for the
   fallback path.
4. CSS: `line-height:1` on `.t` and on the nested-glyph `placement` constant.

## Tests / verification

- Perceptual-diff oracle (stage-4 item 4.18) is the natural gate once it lands;
  until then, eyeball
  `cmake-build-relwithdebinfo/test/output/odr-public/output/pdf/style-various-1.pdf/document.html`
  — text should sit inside its highlight boxes, not below them.
- Add a focused unit-style check: a known run at a known PDF origin emits a
  `top` shifted up by `ascent_em * size` (uniform branch) and a matrix with the
  ascent folded in (matrix branch).
- Regression: a font with no embedded program and no descriptor ascent still
  renders (constant `0.8` em), and an upright run still uses the cheap
  `left`/`top` branch (the fix must not force the matrix branch).

## Open questions

- Prefer FontDescriptor `/Ascent` or the embedded font's `OS/2` ascender when
  they disagree? The descriptor states intent; the embedded metric matches what
  the browser draws. Leaning toward the embedded metric for the embedded-font
  path (placement matches rendering) and the descriptor only for the fallback
  path — settle when implementing.
- Composite/CID fonts: confirm the chosen ascent source is per-font, not
  per-CID; the em fraction should hold for the whole font.
- Does `line-height:1` interact badly with the dual-layer overlay (parent
  transparent Unicode layer vs nested PUA glyph layer)? Both must shift
  identically; since the nested layer inherits placement via `.gvN`/`.giN`,
  the shift must live on the parent or on both consistently.
