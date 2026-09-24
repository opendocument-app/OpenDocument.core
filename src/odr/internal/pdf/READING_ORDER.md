# PDF selection-layer reading order

Status: not implemented. The selection lines stay in content-stream order
(see the comment at the end of the selection-layer pass in
`html/pdf_file.cpp`).

## Problem

The selection layer in `html/pdf_file.cpp` emits one transparent, absolutely
positioned line block per text line. The PDF geometry places the blocks, so a
drag-select highlights the right boxes whatever the DOM order. DOM order
decides only the copy-and-paste order and find-in-page.

The lines are emitted in the order the content stream paints them. That is
wrong whenever paint order differs from reading order. Two cases pull in
opposite directions:

- Out-of-order single column. A generator paints lines out of top-to-bottom
  order (late headers and footers, form fields, tables). A sort by baseline y
  fixes it.
- Multi-column. The stream emits one column fully before the next, so stream
  order is correct. A sort by y interleaves the columns.

No scalar sort key satisfies both, because reading order is a property of the
2-D layout.

## Proposed fix: recursive X-Y cut

1. Take the bounding boxes of all runs on the page.
2. Project them onto the X axis and find the widest vertical whitespace
   gutter. If one exceeds a threshold, cut there: that is a column boundary.
   Recurse into each side, ordered left to right.
3. Otherwise project onto Y and cut at the widest horizontal gap. Recurse top
   to bottom.
4. At the leaves the runs are in reading order. Concatenate.

X cuts separate columns before any Y ordering can interleave them, and Y cuts
order the single-column case. `pdftotext` and PDF.js use the same approach.
It is about 100 lines.

A lighter first step is one pass of column detection: build an X-coverage
histogram per page, find the vertical gutters, bucket the runs into columns,
sort by y within each column, and order the columns left to right. Use the
recursive version only if a reference document needs it.

### Writing direction and rotation

The cut tree is direction-agnostic. Only the child ordering carries direction,
as three parameters per region: the line-advance axis, its sign, and the
block-advance sign. RTL orders X-cut children right to left. Vertical CJK swaps
the roles of X and Y and reverses the column order. Rotated text cuts in a
frame rotated by the dominant glyph-advance angle. The glyph-advance geometry
gives all of these per region. Mixed direction within a line is the Unicode
Bidi Algorithm on the logical text, which is orthogonal to segmentation.

### Where X-Y cut fails

Non-Manhattan layout: text wrapping a floating figure, columns of unequal
length with no clean horizontal divide, staggered or overlapping blocks. There
the greedy first cut can be wrong, or no straight cut exists.

## Complement: bottom-up clustering

Merge runs into lines by x distance, then lines into blocks by y distance.
Grouping is local and direction-independent, so it survives some non-Manhattan
layouts. But it solves grouping, not ordering. Within a block a y sort is safe,
because a block is single-column. Between blocks the column problem returns.
So clustering is best used as a reducer: agglomerate the runs into a few block
boxes, then run X-Y cut on the blocks. Thresholds must scale with the local
font size or median line height, never be absolute, or a narrow gutter welds
two columns into one line.

## What stays the same

Intra-line spacing and run joining (sort by x within a line, decide spaces
from the real neighbour's geometry) are orthogonal to line ordering and are
already correct. Segmentation changes only the order in which whole lines are
emitted.
