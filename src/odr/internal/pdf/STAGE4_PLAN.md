# PDF stage 4 — graphics: PR-by-PR plan

Goal of stage 4: render the **non-text** imaging model — vector paths, images,
shadings, patterns — plus the two font cases deferred from stage 3 (Type3,
non-embedded). Everything is **serialized to SVG/HTML, never rasterized**
(decision 2026-06, recorded in `AGENTS.md`): the browser draws it. pdf.js proves
no native renderer is needed; the PDF and SVG imaging models are close cousins.

## Two foundational decisions (fixed for this plan)

1. **Painter-order interleave.** Today `extract_text` emits a text-only
   `std::vector<TextElement>`. Stage 4 generalizes this to **one ordered element
   stream in paint order** — text, path-paint, image, shading events — so a fill
   painted after text occludes it and vice versa. The HTML layer walks that one
   list and emits interleaved `<svg>` fragments and text `<span>`s. This is the
   faithful model and avoids a painful re-refactor later; the cost is that 4.0
   reshapes the current pipeline.

2. **Text stays HTML, graphics become SVG.** We keep stage 3's dual-layer
   `@font-face` text spans for selectable/searchable text. SVG carries only
   non-text imaging (and Type3 glyphs, which have no font program). We do **not**
   render normal glyphs into SVG.

Each sub-stage is one PR (a few are split where noted). They are dependency
ordered; 4.0–4.3 are the load-bearing refactor + first visible output, 4.4+ widen
coverage. The perceptual-diff oracle (4.18) can land any time after 4.2 and should
land early enough to guard the rest.

---

## 4.0 — Page-element IR + path accumulation (refactor, no new output)

The linchpin. No visible change; pure plumbing + tests.

- Introduce a renderer-agnostic **`PageElement`** model (variant / small class
  hierarchy) with `TextElement` becoming one case. Add `PathElement` (geometry +
  paint intent) as the second.
- Rename `extract_text` → `extract_page` (or add it and reimplement
  `extract_text` on top, then migrate callers) returning
  `std::vector<PageElement>` in paint order. `html/pdf_file.cpp` keeps working by
  handling only the text case for now.
- **Accumulate path geometry in `GraphicsState`/the executor.** The construction
  operators (`m l c v y h re`) are tokenized today but their geometry is
  discarded (`Path::current_position` only). Build subpaths (lists of
  line/cubic segments + closed flag) in user space by applying the CTM at
  construction time (per ISO 32000-1 8.5.2.1 the CTM in effect when the point is
  added). Bézier variants `v`/`y` expand to full cubics.
- On a **paint operator** (`S s f F f* B B* b b* n` + clip variants `W W*`),
  emit a `PathElement` carrying: the subpaths, paint kind (none/fill/stroke/
  fill+stroke), fill rule (nonzero/evenodd), and a snapshot of the paint-relevant
  state (stroke/fill color slots as-is for now, line width, caps/joins/miter/
  dash, plus the current clip — see 4.3). Then reset the path. `n` with a pending
  `W` emits a clip-only element.
- Tests: inline content streams through `extract_page`, asserting subpath
  geometry under `cm`/`q`/`Q`, rectangle expansion, Bézier expansion, the paint
  matrix of emitted elements, and that text + path elements come out in paint
  order.

Deliverable: IR only. Visual output unchanged.

## 4.1 — SVG scaffolding + filled paths (first pixels)

- Per page, the HTML layer walks the ordered `PageElement` list and, for runs of
  graphics elements, opens an `<svg>` positioned over the page box with a
  `viewBox` in PDF user space (y-up handled by the SVG transform, matching the
  existing text mapping) so path coordinates need no per-point flip.
- Render **filled** `PathElement`s: subpaths → one `<path d=...>`,
  `fill-rule: nonzero|evenodd`, `fill` from the current non-stroking color
  (DeviceGray/RGB/CMYK → RGB only for now; CMYK via the naive
  `255*(1-c)*(1-k)` conversion, refined in 4.4).
- Interleave with text spans per the painter-order list (decision 1).
- Tests: a known fill (rect, triangle) produces the expected `<path>` + color;
  evenodd vs nonzero; a fill behind/in-front-of a text span lands on the right
  side in the emitted DOM order.

## 4.2 — Strokes & line style

- Render **stroked** paths: `stroke` color, `stroke-width` (transformed by the
  CTM — handle non-uniform scaling; for a non-uniform CTM SVG can't express an
  anisotropic pen, so approximate with the average scale and note the limit),
  `stroke-linecap` (`0/1/2` → butt/round/square), `stroke-linejoin`
  (`0/1/2` → miter/round/bevel), `stroke-miterlimit`, `stroke-dasharray` +
  `stroke-dashoffset` from the dash pattern.
- Fill+stroke paint ops (`B`/`b` family) emit both.
- Move the lingering stdout/stderr debug in `pdf_graphics_state.cpp` (dash
  pattern, stroke/other color) onto `Logger` while here (cross-cutting cleanup).
- Tests: each line param → attribute; dash pattern round-trip; fill+stroke emits
  both paint attributes.

## 4.3 — Clipping

- Track the **clip path** in the graphics state: `W`/`W*` set a pending clip that
  the next paint/`n` op installs as the intersection with the current clip; the
  clip is part of saved/restored state (`q`/`Q`) and is snapshotted onto each
  emitted `PathElement` (added to 4.0's snapshot).
- HTML: emit nested `<clipPath>` elements and reference them via `clip-path`.
  Intersection of multiple clips → nested clip paths. Form-XObject `/BBox`
  clipping (deferred in stage 2) also lands here, since it's the same machinery.
- Tests: a clip rect limits a later fill; nested clips intersect; `q`/`Q`
  save/restore the clip; form `/BBox` clips its content.

## 4.4 — PDF function evaluator + color spaces

Shared prerequisite for proper color **and** shadings, so it lands before both.

- **4.4a — Function evaluator (`pdf_function`).** Evaluate PDF functions
  (ISO 32000-1 7.10): type 2 (exponential), type 3 (stitching), type 0 (sampled,
  with interpolation), type 4 (PostScript calculator — a small stack-machine
  interpreter). Pure, unit-testable in isolation against spec examples.
- **4.4b — Color spaces (`pdf_color`).** Resolve `/ColorSpace` resources and the
  `CS`/`cs`/`SC`/`SCN`/`sc`/`scn` operators (today only the device color ops are
  modelled). Convert to RGB at emit time: `ICCBased` (approximate by component
  count / `/Alternate`, treat as sRGB), `CalRGB`/`CalGray` (≈ device),
  `Lab` (→ XYZ → sRGB), `Indexed` (palette lookup), `Separation`/`DeviceN`
  (sample the tint transform via 4.4a). Overprint ignored. Wire into the path
  fill/stroke color resolution from 4.1/4.2.
- Tests: each function type against spec vectors; each color space → expected
  RGB; `scn` with a Separation space.

## 4.5 — Image XObjects: JPEG pass-through (`DCTDecode`)

- The filter framework already hands back `DCTDecode` payloads undecoded
  (`stopped_at_filter`). Detect an `/XObject /Image` invoked by `Do`, emit an
  **`ImageElement`** (new `PageElement` case) referencing the raw JPEG bytes;
  HTML emits `<image>` (inside the page `<svg>`, placed by the CTM — a unit
  square maps to the image rectangle) with a `data:image/jpeg;base64` href.
- Honor `/ImageMask` false here only; masks come in 4.8.
- Color-key/`/Decode` and CMYK JPEGs flagged as follow-ups (Adobe CMYK JPEG
  inversion is a known wrinkle).
- Tests: a `Do` of a DCTDecode image emits an `ImageElement` at the CTM rect with
  the right bytes; round-trip the base64.

## 4.6 — Raster images: Flate/LZW → PNG encode

- For images whose data **is** decoded (Flate/LZW/raw), assemble samples using
  `/Width`/`/Height`/`/BitsPerComponent`/`/ColorSpace` (reuse 4.4b) and PNG-encode
  to RGB(A). Needs a minimal PNG writer (zlib already available via the filter
  framework's inflate dependency — confirm a deflate path exists or add one).
- `/Decode` arrays, 1/2/4/8/16 bpc, indexed palettes.
- Tests: a small known raster (e.g. 2×2 indexed) → expected PNG pixels.

## 4.7 — Inline images (`BI`/`ID`/`EI`)

- Fix the tokenizer first: `pdf_graphics_operator_parser` currently can't scan
  past `ID` correctly (the binary payload isn't length-delimited; scan to the
  `EI` delimiter respecting whitespace + the abbreviated keys). The filter
  abbreviations are already handled by `pdf_filter`.
- Map abbreviated keys (`/W /H /BPC /CS /F /IM /D`) to their long forms and route
  through the same image emission as 4.5/4.6.
- Tests: an inline image tokenizes as one operator with its dict + bytes; a
  Flate inline image emits the expected `ImageElement`.

## 4.8 — Image masks, stencil masks, soft masks (`SMask`)

- `/ImageMask true` (1-bpc stencil): paint the current fill color through the
  mask → SVG `<image>` + `<mask>` or a recolored PNG with alpha.
- Explicit `/Mask` (color-key array or a mask image) and `/SMask` (alpha image):
  composite into the RGBA PNG, or emit an SVG `<mask>`.
- Tests: a stencil mask paints the fill color; an SMask produces alpha.

## 4.9 — Axial & radial shadings (types 2/3)

- `sh` operator and shading **patterns** (`scn` with a `/Pattern` color space
  naming a `/PatternType 2` shading pattern).
- Type 2 (axial) → `<linearGradient>`, type 3 (radial) → `<radialGradient>`,
  with stops sampled from the shading's `/Function` (4.4a) across `/Domain`,
  `/Extend` → `spreadMethod`/explicit end stops, `/Coords` mapped through the CTM
  (and the pattern matrix). Background/`/BBox` honored.
- Tests: an axial shading → gradient stops + coords; `/Extend` handling; a
  shading pattern fills a path.

## 4.10 — Tiling patterns (`PatternType 1`)

- Reuse the form-XObject machinery (stage 2): a tiling pattern's content stream
  is a mini page run. Emit an SVG `<pattern>` tile (its own nested element list,
  recursively rendered) sized by `/XStep`/`/YStep` and the pattern `/Matrix`;
  reference it as `fill="url(#...)"`. Colored (`/PaintType 1`) vs uncolored
  (`/PaintType 2`, painted in the current color) variants.
- Cycle/recursion guard analogous to the form active-set guard.
- Tests: a tiling pattern emits a `<pattern>` with the tile content; uncolored
  pattern picks up the current color.

## 4.11 — Mesh & function-based shadings (types 1, 4–7)

The SVG-residue long tail; lowest priority, rare in the corpus.

- Tessellate into many small flat-shaded polygons (pdf.js's approach): type 1
  (function-based) sampled over its domain, types 4–7 (Gouraud triangle meshes,
  Coons/tensor patches) decoded from their data streams into triangles → flat
  `<polygon>`s with averaged color. Coons/tensor patches flattened to triangles.
- Tests: a type-4 triangle mesh → the expected polygons; a type-1 function
  shading tessellates.

## 4.12 — Non-embedded font substitution + standard-14 AFM widths

Deferred from stage 3. Independent of the SVG path — can be parallelized.

- A font with no `/FontFile*`: map the standard 14 (Helvetica/Times/Courier +
  Symbol/ZapfDingbats) and common names to CSS `font-family` fallback stacks
  (serif/sans/mono chosen by `/Flags` + name; bold/italic from `/Flags` and the
  name). The glyph shapes are the browser's fallback font; text stays in the
  existing span path (no SVG).
- **AFM widths** for the standard 14 (which usually ship no `/Widths`) as a
  generated data table (`tools/pdf/generate_afm_data.py` → `pdf_afm_data.*`),
  feeding `Font::advance_width` so placement is correct. Closes the deferred
  "AFM widths" item from stage 2.
- Tests: a bare `/BaseFont /Helvetica` font resolves a family + AFM widths; flags
  → bold/italic/serif selection.

## 4.13 — Type3 fonts

Deferred from stage 3; depends on the path→SVG machinery (4.1–4.4) since Type3
glyphs are mini content streams.

- A Type3 font's `/CharProcs` are content streams (already tokenizable). For each
  shown code, run the char proc through the executor (it emits paths/images via
  the stage-4 machinery) placed at `CTM × Tm × /FontMatrix` (font size folded
  in), against the font's `/Resources`. `d0`/`d1` already modelled.
- No glyph program → no PUA re-encode, no reverse map: the dual-layer model holds
  (SVG glyph layer + transparent Unicode span from the code→Unicode chain for
  selection).
- Tests: a Type3 glyph emits its char-proc paths at the glyph transform; Unicode
  still extracted for selection.

## 4.14 — Transparency: constant alpha & blend modes

- `/ExtGState` `CA`/`ca` → `stroke-opacity`/`fill-opacity` (or group `opacity`);
  the `gs` operator already lands in state as `graphics_state_parameters` (a name
  today) — resolve the actual `/ExtGState` dict.
- Blend modes (`/BM`) → CSS `mix-blend-mode` where it maps; unmappable modes
  (rare) fall back to normal with a `Logger` note.
- Tests: `ca` → fill-opacity; a known `/BM` → the mapped blend mode.

## 4.15 — Transparency groups & soft masks (`SMask` in ExtGState)

- Luminosity/alpha soft masks (`/SMask` in `/ExtGState`, a transparency group
  XObject) → SVG `<mask>`. Isolated/knockout groups don't map cleanly — punt with
  a `Logger` note (rare), per the AGENTS sketch.
- Tests: a luminosity soft mask → a `<mask>` applied to following content.

## 4.16 — Annotation appearance streams (optional pull-forward from stage 5)

Not core stage 4, but it reuses the form-XObject + path machinery and is cheap
once 4.1–4.13 exist; can be deferred to stage 5. Listed here only as a candidate.

## 4.17 — Coverage hardening + AGENTS.md rewrite

- Sweep the corpus (odr-public + the PDF101 "nasty files"), fix the long tail of
  operator gaps surfaced by the oracle, and **rewrite the `pdf/AGENTS.md`
  stage-4 section** from "roadmap" to "what works" (as stages 0–3 did on close),
  dropping any stage tags from code. Delete this plan file.

## 4.18 — Perceptual-diff test oracle (land early, after 4.2)

Parallels stage 3's OTS gate. **Not a runtime dependency.**

- CI-only: render corpus fixtures with poppler **or** pdf.js, screenshot our HTML
  output (headless Chromium), perceptual-diff against the reference, gate on a
  threshold. Lives in `test/` / CI config, off the shipped build.
- Land this right after 4.1/4.2 so every subsequent graphics PR is guarded.

---

## Suggested merge order

`4.0 → 4.1 → 4.2 → 4.18 (oracle) → 4.3 → 4.4 → 4.5 → 4.6 → 4.7 → 4.8 → 4.9 →
4.10 → 4.13 → 4.14 → 4.15 → 4.11 → 4.16? → 4.17`

`4.12` (non-embedded fonts) is independent of the SVG path and can land any time
in parallel. The high-value visual milestone is **4.0–4.6** (vector paths, color,
JPEG + raster images) — most real PDFs look right after that; 4.9–4.15 are the
fidelity tail.

## Open questions to settle per-PR (not blockers now)

- Where the page `<svg>` boundaries fall when graphics and text interleave many
  times — one `<svg>` per contiguous graphics run vs one per page with text
  woven through it as `<foreignObject>`. Lean: contiguous-run `<svg>` fragments
  between span groups (keeps text as real HTML for selection).
- CMYK conversion fidelity (naive vs ICC-approximated) — start naive (4.1),
  revisit in 4.4 if the oracle demands.
- PNG writer: confirm a deflate encoder is reachable from the existing zlib dep
  before 4.6, else add a minimal one.
