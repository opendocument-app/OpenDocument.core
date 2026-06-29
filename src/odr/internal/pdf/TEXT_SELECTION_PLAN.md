# PDF text selection & search — plan

## Context

The PDF → HTML imaging output is now quite complete (stage 4), but **marking
text and searching is poor**. The cause is structural: every show-text segment
(`Tj`, or one string of a `TJ` array) becomes one `pdf::TextElement`
(`pdf_page_element.hpp`), and each becomes one **absolutely-positioned `<span>`**
placed at its run origin (`html/pdf_file.cpp` ~`l.902–941`). Runs that share a
visual line are therefore independent boxes at arbitrary coordinates with no
whitespace or reading order between them, so:

- find-in-page can't match a phrase that crosses a run boundary, and a single
  word split by kerning (several `TJ` adjustments) is several spans, so even
  one-word search misses;
- dragging a selection jumps between unrelated boxes — there is no line flow for
  the browser to follow;
- copy order follows content-stream (paint) order, not always reading order.

For text with an embedded font we already emit a **dual layer** — a transparent,
selectable Unicode parent span with a nested visible PUA-glyph span
(`html/pdf_file.cpp` ~`l.1020–1054`). The selectable text already exists; it is
just **per-run and absolutely positioned**, which is exactly what selection and
search need it not to be.

Intended outcome: native, JS-free selection and find-in-page that flow along
lines (and across wrapped lines within a block), without regressing the
pixel-perfect visual rendering.

## Approach (decided)

Keep the **visual glyph layer exactly as it is** — absolutely-positioned PUA
spans are what make the rendering good; do not touch them. Restructure **only
the transparent Unicode layer** into per-line (and, where confident, per-block)
containers in reading order, with real whitespace between runs. This is
PDF.js-style layering done **statically at generation time**.

Fixed decisions from discussion:

1. **Separate text layer**, not a unified reflow. The selectable Unicode becomes
   its own line/block-grouped layer; the visual layer stays pixel-perfect and
   independent. The selection layer **replaces** today's per-run transparent
   spans rather than stacking on top of them, so node count stays roughly flat.
2. **Static HTML/CSS only — no JavaScript.** Because we translate ahead of time,
   all layout analysis happens in the C++ pass and is baked into static DOM;
   native browser selection and Ctrl+F then work with zero runtime. This is a
   hard constraint (JS-free output is a core virtue of the current export).
3. **Eager to split, conservative to merge.** Cluster runs into lines by baseline
   (y + orientation), order by x. Merge adjacent lines into a block only when
   clearly the same column — overlapping x-range, consistent leading
   (baseline-to-baseline ≈ font size), same writing direction, same-ish size.
   When any signal is shaky, fall back to separate line containers. The fallback
   is lossless for within-line UX, so multi-line grouping never hurts the
   single-line case. This also makes **tables safe without table detection**:
   cells never merge across columns, so they fall out as separate line
   containers (correct selection) — we do not need to recognize the table.
4. **Multi-line is the target**, degrading gracefully to line-only. Intra-block
   line breaks get a single space separator so search matches across the wrap
   (`"the\nquick"` → findable `"the quick"`).

## Mechanism

### Grouping (generation time, C++)

A linear sweep over the page's `TextElement`s **in content-stream order** —
crucially *not* a global re-sort. Content-stream order is almost always reading
order already (a producer draws a column top-to-bottom, then the next column), so
trusting it is what keeps multi-column text and tables from scrambling. A global
sort by (baseline, x) would interleave columns sharing a y-band — exactly the
failure we must avoid.

The sweep tracks the previous run's baseline and right edge, and for each run
decides the **separator** to insert before it:

1. New line when the baseline jumps (more than ~0.6·font-size) or the run starts
   left of where the previous ended (a column/line break in producer order).
2. Same-line gap when the horizontal gap to the previous run exceeds a small
   fraction (~0.25·font-size).

Either case inserts a single space (so search matches across the break),
*unless* the adjacent text already carries whitespace — many gaps are already
represented as an inferred leading space on the segment (`TextElement::text`), so
guard against double spaces (which would break literal find-in-page).

Cost is O(n) per page — negligible next to the embedded-font re-encode that
already dominates the pass. No new font work; no sort.

### Within-line gaps → whitespace (the core tension)

Between consecutive runs on a line there is a horizontal gap (inter-word space,
kerning, tab to a column). The selection layer must reconcile two conflicting
needs:

- **Searchable whitespace** — a real space character makes `"the quick"`
  findable and copy readable, but a literal space has the font's space-width, not
  the exact PDF gap, so the transparent text **drifts** from the glyphs along the
  line.
- **Positional accuracy** — an exact-width spacer (inline-block / letter-spacing)
  stays aligned but carries no whitespace, so words run together (`"thequick"`)
  and search breaks.

No single trick gives both, and the obvious width-fix is **not available to us**:

- **`transform: scaleX(...)`** (the PDF.js technique) needs the run's *rendered*
  width to compute the scale, which PDF.js measures **at runtime in JS**. We emit
  no JS, so statically the factor is either uncomputable (system-font selection
  layer) or just `1` (embedded font — the advances already match). So `scaleX`
  buys us nothing and is dropped.
- **Chosen approach: anchor every run at its own known origin.** Each selection
  span is absolutely positioned at the run's origin (the placement we already
  compute for the glyph layer), so drift can only accumulate *within* one short
  run, never across the line. Real spaces between runs (see grouping) give
  searchable whitespace; the per-run anchoring keeps the highlight close without
  any width-fix. Fully static, no JS.
- **Dead end — do not pursue: `position: relative; left:`** to offset an inserted
  space. Relative positioning shifts the box visually but leaves its space
  reserved in the flow, so siblings don't move; it cannot reclaim the gap. (A
  negative `margin-left` *would* reclaim it, but with per-run origin anchoring we
  don't need to.)

### Highlight alignment quality bar

Selection highlights the *selection layer's* boxes; if those are offset from the
visible glyphs the highlight looks shifted (text itself is never wrong — the
glyph layer is a separate, perfect layer, so misalignment shows up only as a
slightly-off highlight rectangle during an active drag). Per-run origin anchoring (above)
keeps this in the acceptable band: each run's highlight starts exactly on its
glyphs and can only drift within that one short run. We ship that and revisit
only if the residual within-run drift is noticeable in practice.

## De-hyphenation (tracked, deferred)

A line-final hyphen (`"infor-\nmation"`) is unfindable as `"information"` whether
joined with a space (`"infor- mation"`) or nothing (`"infor-mation"`); only
dropping the hyphen + break fixes search. But it is genuinely ambiguous — a
line-final hyphen may be a soft break hyphen (`infor-mation`) or a real one
(`well-\nknown` must stay `well-known`), and PDF almost never marks the
difference (most producers emit plain `U+002D` for both; the rare `U+00AD` soft
hyphen is the only unambiguous signal).

Decision: **do not auto-de-hyphenate in v1** — lossy and wrong often enough to be
a net negative for copy fidelity. Join intra-block lines with a space, accept
that hyphenated-across-line words miss in search. Revisit as an opt-in heuristic:
collapse only when the trailing char is `U+00AD`, or behind a config flag.

## Implementation sketch

All changes are in the HTML layer; the IR (`pdf::TextElement`) already carries
what we need (`transform`, `size`, `advances`, `text`).

- **`src/odr/internal/html/pdf_file.cpp`** — `HtmlServiceImpl::write_document`:
  - **Visual layer (paint order, non-selectable):** every embedded-font run emits
    its PUA glyph span (the existing display-only form, `base + " g " + fvN`);
    fallback runs emit their Unicode in the system font, also `.g`. All visual
    text is `user-select:none`. Invisible runs (Tr 3/7) paint nothing, so they
    emit **no** visual span at all.
  - **Selection layer (content order, transparent, selectable):** any run with
    extractable text contributes one span carrying the real Unicode, anchored at
    the run origin (reuse `base`), transparent via `.i`, with the leading
    separator from the grouping sweep. Emitted per page after the paint content
    so the selectable spans are contiguous in the DOM.
  - **Fold out the "collapse" path.** Because all common-case selectable text now
    lives in the selection layer, the visible layer no longer needs to render
    real Unicode in the embedded font. Remove the collapse machinery
    (`collapsible_unicode`, `used_unicode`, the per-run first-wins scalar walk)
    and the real-Unicode `cmap` baking in the post-pass — the font is re-encoded
    **PUA-only** (`reencode_to_pua(*sfnt)` / `wrap_to_otf(*cff)` with no extras).
    Visual output stays pixel-identical (PUA maps to the same glyphs); the DOM
    and font subset shrink in complexity.
  - Separator classes are not needed (spaces ride inside the span text); existing
    placement classes are reused via the `AtomicStyles` interner.

## Size / performance notes

- Generation: +O(n log n) sort per page; negligible.
- HTML size: the Unicode bytes are **relocated**, not duplicated; node count
  stays roughly flat (one container per line vs. a transparent parent per run).
  `scaleX` classes are bounded by distinct values via `AtomicStyles`. Real-space
  separators are ~free; prefer them over inline-block spacers. The docs that
  approached GitHub's 100 MB reference-output ceiling are dominated by the glyph
  layer + embedded fonts, both unchanged. (The 100 MB ceiling is a soft
  reference-output constraint, not a product limit — keep the layer lean but do
  not let it block the design.)

## Verification

- Build/test in `cmake-build-relwithdebinfo`; run the PDF HTML output tests and
  regenerate reference output for the `test/data/.../output/pdf` fixtures, eyeing
  the diff for size and structure.
- Manual: open representative outputs (e.g. `geneve_1564.pdf`,
  `978-3-030-65771-0.pdf`, a multi-column doc) in a browser and check:
  - Ctrl+F finds phrases that span run boundaries and wrapped lines.
  - Click-drag selection flows along a line and across lines in a block.
  - Copy yields readable, correctly-ordered text.
  - Multi-column / table-like pages do **not** scramble across columns on copy.
  - Visual rendering is byte-for-byte unchanged from before (glyph layer
    untouched) — confirm via the perceptual-diff oracle.
  - Output stays JS-free.

## Future work

- De-hyphenation heuristic (opt-in / `U+00AD`-only).
- Gap-based word separators within a line (beyond the producer's inferred
  spaces), if word-merging shows up in practice.
- Richer static structure recovery (semantic `<table>` / multi-column markup) —
  a separate, larger layout-analysis effort, out of scope here.
