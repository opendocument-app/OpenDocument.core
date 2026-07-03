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

### What stays the same

Intra-line spacing and run joining (sort by *x* within a line, decide spaces and
joins from the real x-neighbor's geometry) is orthogonal to line ordering and is
already correct — segmentation only changes the order in which whole lines are
emitted.

## Status

Not implemented. Selection lines are emitted in content-stream order as an
interim state (see the comment at the end of the selection-layer pass in
`pdf_file.cpp`).
