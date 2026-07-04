# PDF selection-layer reading order

## Problem

The HTML selection layer (`internal/html/pdf_file.cpp`) emits one transparent,
absolutely-positioned line block per text line. These blocks are laid out
visually by their PDF geometry, so the browser highlights the right boxes on
drag-select regardless of DOM order. DOM order only affects **copy/paste text
order** and **find-in-page**.

We currently keep the selection lines in **content-stream order** — the order
the PDF content stream paints them. This is wrong whenever paint order does not
match reading order.

Two failure modes, pulling in **opposite** directions:

- **Out-of-order single column.** A generator paints lines out of visual
  top-to-bottom order (late-painted headers/footers, form fields, tables).
  Stream order copies them out of order; sorting by baseline *y* fixes it.
- **Multi-column.** The stream almost always emits one column fully before the
  next, so stream order keeps columns contiguous and correct. Sorting by *y*
  interleaves the columns and destroys reading order (and eagerly joins runs
  across the gutter with a space).

An earlier revision sorted lines by quantized baseline *y* after collecting a
page. That fixed the single-column case but regressed multi-column, so it was
removed (commit on `pdf-text-selection`) in favor of plain stream order. Stream
order regresses the single-column case instead.

**Neither is correct**, and no scalar sort key can be: reading order is a
property of the 2-D page layout, not of any single coordinate. `y` alone breaks
columns; stream order breaks out-of-order pages; lexicographic `(x, y)` /
`(y, x)` each break one of the two. You cannot flatten a columned page into a
1-D sort.

## Proposed fix: recursive X-Y cut (XY-cut)

The classic, well-bounded page-segmentation algorithm. It recovers reading order
by recursively splitting the page at whitespace gutters:

1. Take the bounding boxes of all runs on the page.
2. Project them onto the **X** axis; find the widest vertical whitespace gutter.
   If one exists above a threshold, cut there — that is a **column boundary**.
   Recurse into each side, ordered left-to-right.
3. Otherwise project onto **Y**, cut at the widest horizontal gap → rows.
   Recurse top-to-bottom.
4. At the leaves the runs are already in reading order; concatenate.

One mechanism handles both failure modes: X cuts separate columns *before* any
Y ordering can interleave them, and Y cuts order out-of-order single-column
content. It is the standard approach in document-layout tooling (poppler's
`pdftotext`, PDF.js) and is on the order of ~100 lines — not a text-extraction
engine.

### Lighter first step

If full XY-cut is more than the reference set needs, start with **single-pass
column detection**: build an X-coverage histogram once per page, find the
vertical gutters, bucket runs into columns, then sort by *y* **within** each
column and order columns left-to-right. This handles the overwhelmingly common
"N parallel columns" case; XY-cut additionally handles nested / irregular
layouts. Reach for the recursive version only if a reference document needs it.

### Writing direction, rotation, and bidi

XY-cut is often described as LTR-top-to-bottom only, but that is a property of the
**child ordering**, not of the algorithm. The cut *tree* ("find the widest gutter,
split, recurse") is direction-agnostic. Only steps 2–3 — "order left-to-right",
"order top-to-bottom" — carry direction. So a writing mode is not a different
algorithm; it is three parameters per region: the line-advance axis, the
line-advance sign, and the block-advance sign.

- **LTR horizontal.** X-cut children L→R, Y-cut children top→bottom (as written).
- **RTL (Arabic/Hebrew).** Same tree; X-cut children ordered R→L and the intra-line
  x-sort descends.
- **Vertical CJK.** Glyphs advance downward within a line and lines advance
  right-to-left: X and Y swap roles *and* the column order reverses.
- **Rotated 90°/270°.** Rotate the region's coordinate frame by the dominant
  glyph-advance angle, then cut in the rotated frame.

We can **infer** these per region from the glyph-advance geometry we already have
(successive glyph positions within a run give the advance axis and sign); PDF
rarely states writing mode explicitly. "Bottom-to-top" is not a real script
direction — it only appears as rotated or vertical text and is covered by the
frame-rotation case.

Mixed direction *within a line* (Arabic with embedded Latin or digits) is not a
layout problem at all — it is the Unicode Bidi Algorithm on the logical text, and
is orthogonal to segmentation. Copy order should stay logical order.

### Where XY-cut still fails

The clean **2-column + 1-column** case is exactly what XY-cut solves, not a
weakness: a full-width banner over two columns is a Y-cut (banner | column region)
followed by an X-cut (col | col); a footer spanning under two columns is the same
in reverse. XY-cut's real limitation is **non-Manhattan** layout — regions not
separable by any single full-width or full-height straight whitespace cut:
L-shaped text wrapping a floating figure, columns of unequal length where no
horizontal line cleanly divides the 1-column part from the 2-column part,
staggered or overlapping blocks. There the greedy first cut can be wrong, or no
straight cut exists at all. Mitigations (Breuel whitespace-rectangle cover,
backtracking cuts) are heavier than the reference set is likely to need.

## Alternative / complement: bottom-up clustering

A greedy agglomerative pass — merge runs into lines by *x*-distance, then merge
lines into blocks by *y*-distance — is the **dual** of XY-cut: bottom-up region
growing instead of top-down splitting (this is what PDFMiner's `LAParams` and
older Tesseract layout do). Its grouping step is direction-independent (distance
is symmetric) and *local*, so it survives some of the non-Manhattan layouts that
defeat a global gutter.

But clustering solves **grouping, not ordering**, and the two ordering
subproblems then split cleanly:

- **Within a block** a y-sort is safe *by construction* — a block is single-column,
  so there are no columns to interleave. Intra-block ordering is a trivial,
  sign-flipped-per-direction y-sort.
- **Between blocks** is the columns problem again; ordering the blocks re-derives
  XY-cut's output.

So clustering is best used not as a replacement for XY-cut but as a **reducer that
feeds it**: agglomerate thousands of runs into a handful of block boxes, then run
XY-cut (or a simple column sweep) *on the blocks*. Gutter-finding that is fragile
on raw runs is trivial on a few large, well-separated blocks, and the result
inherits clustering's tolerance for irregular layouts. This is the most promising
direction if the simpler passes above prove insufficient.

Its own failure mode is threshold sensitivity: a fixed *x*-gap threshold that
bridges a narrow gutter welds two columns into one "line" — the multi-column
failure relocated into a constant. Thresholds must scale to local font size /
median line height, never be absolute.

### What stays the same

Intra-line spacing and run joining (sort by *x* within a line, decide spaces and
joins from the real x-neighbor's geometry) is orthogonal to line ordering and is
already correct — segmentation only changes the order in which whole lines are
emitted.

## Status

Not implemented. Selection lines are emitted in content-stream order as an
interim state (see the comment at the end of the selection-layer pass in
`pdf_file.cpp`).
