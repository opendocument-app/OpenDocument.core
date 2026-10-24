# Lazy loading design

Status: proposal. Nothing of it has landed. Issue #764.

Related: [`editing.md`](editing.md) holds the editing mode, which a page that
drops content must keep working. [`txt-editing.md`](txt-editing.md) holds the
editor of the first view this design changes.

## Problem

A large file fails in two places, and the browser fails first.

- **The browser.** A page that states every line, row or page as DOM costs
  layout and memory for content the reader never sees. A text file of one
  million lines paints its first frame in Chrome after 154 s, and its
  renderer holds 6.3 GB.
- **Core.** A decode holds the whole document. `efficiency-big-1.ods` (1.4 MB
  zipped, 297 MB of `content.xml`) peaks at 697 MB in `translate`, before the
  page reaches a browser.

The html limits (`spreadsheet_limit`, `spreadsheet_cell_limit`) bound what is
written, not what is read. They also cut the content, which a reader does not
want (#740, #757).

## Measurements

### The browser

Headless Chrome, `file://`, one page for each run, 2026-10-24. The text is a
generated log of one million lines (71 MB), 3 to 14 words on each line.
"First frame" is the time until the page first paints. "Layout" and "style"
are the totals of `Performance.getMetrics`. "RSS" is the sum of the renderer
processes, so only the ratios count.

| page | first frame | layout | style | DOM nodes | RSS |
|---|---|---|---|---|---|
| `text.html` as `translate` writes it today | 154 s | 91 s | 7.6 s | 6.0 M | 6.3 GB |
| the same, without its scripts | 91 s | 45 s | 2.8 s | 6.0 M | 5.1 GB |
| one div for each line, in one column | 59 s | 26 s | 3.9 s | 3.0 M | 3.3 GB |
| chunks of 256 lines, `content-visibility: auto` | 13.5 s | 6.3 s | 2.6 s | 4.0 M | 5.0 GB |
| chunks of 256 lines, `hidden` until a script marks them near | 0.85 s | 0.1 s | 0.16 s | 3.0 M | 0.9 GB |
| lines as JS strings, a window of 256 in the DOM | 0.5 s | 0.01 s | 0 s | 539 | 1.2 GB |

- Layout is the cost, not the DOM alone. The `hidden` chunks keep every node
  and still paint 180 times sooner, because the browser lays out only the
  chunks near the viewport.
- `content-visibility: auto` is not enough. On the first frame, Chrome lays
  out each `auto` chunk once to find out whether it is near the viewport.
  A chunk that starts `hidden` costs nothing until the script marks it.
- `text.js` doubles the load today. Its `ResizeObserver` measures the
  line-number gutter against every line.
- `translate` sets `format_html`, so whitespace nodes come between the
  lines. The ratios between the rows stay.

### Core

`efficiency-big-1.ods`, `/usr/bin/time -l`, peak memory footprint:

| step | peak |
|---|---|
| `xml::parse` of `content.xml` alone (6.3 M nodes, 5.3 M attributes) | 423 MB |
| the decode (`DocumentFile::document`) | 652 MB |
| `translate`, which renders the decode | 697 MB |

So the buffer of `content.xml` is 297 MB, the pugixml DOM about 125 MB, and
the registry and the styles about 230 MB. The cell children of #781 are about
85 MB of the registry. The render adds 45 MB.

## Decisions

### 1. Three levels, and each one works without the next

| level | what it does | where it works |
|---|---|---|
| **1. Containment** | The view writes its content in chunks. A chunk has `content-visibility: hidden` until a script marks it near the viewport, so the browser lays out only those. Without the script, a chunk has `auto`. | every page, offline too |
| **2. A window** | The page keeps a chunk far from the viewport as a string, not as DOM, and puts it back as the reader comes near. | every page, offline too |
| **3. On demand** | The page asks the host for a chunk, and core renders only that chunk. | a host that answers the request: the HTTP server, or a binding with a callback |

**Why levels:** level 1 is CSS and a few lines of script, and it keeps the
whole DOM. So `odr.search`, the editor and a copy of a selection see all of
the content. The find of the browser and a screen reader do not see a
hidden chunk. Level 2 removes the DOM cost too, but takes more away
(decision 5). Level 3 is the only level that makes core cheaper, and it needs
a host. A view takes the lowest level that is enough
for the size of its file.

`content-visibility` is in Chrome from 85, Firefox from 125 and Safari from
18. A browser without it ignores the property and lays out every chunk, as
it lays out every line today.

**Offline and server mode:** levels 1 and 2 work in a file that
`bring_offline` writes, because the file carries all the content. Only
level 3 needs a server or a host callback. So most of the work helps an
offline file too. What only a server adds is that core does not render,
hold or send the whole document at the start.

### 2. A chunk is the unit, with a stable index and a size

A view cuts its content along one axis: lines of a text file, rows of a
sheet, pages of a pdf, slides of a deck. A chunk is a run of those, for
example 256 lines. Each chunk has an index from 0, and an index names the
same content in every render of one file.

A chunk states the size it takes before it is rendered, so the scroll
position stays where it is when the chunk arrives:

- A pdf page, a slide and a sheet row have a size in the file. The chunk
  states it exactly.
- A line of a text file that does not wrap has the height of one line. A
  line that wraps does not, so the chunk states an estimate.
  `contain-intrinsic-size: auto <estimate>` makes the browser keep the real
  size after the chunk was rendered once.
- A block of a text document has no size before layout. Text documents come
  last, and may take only the page shape of `text_document_margin`.

### 3. The page reads chunks through one source

```js
// what level 2 and level 3 splice from
odr.chunks.source = {
  count: 3907,
  get: function (index) {}, // a Promise of the chunk's html
};
```

- **In the page (level 2):** each chunk is the text of a
  `<script type="text/x-odr-chunk">`. The parser keeps a text node and builds
  no DOM for it. A `<template>` would build an inert DOM, which costs the
  memory that level 2 saves.
- **From a host (level 3):** `get` asks the HTTP server for a chunk path, or
  calls a function that the binding installs.

The splice and the eviction are one script, whatever the source is.

**Why one source:** the bindings render into a web view and only some run a
server. So the chunk API of core is the primitive, and HTTP is one way to
carry it.

### 4. Core writes a chunk of a view

`HtmlView` gets the number of chunks and a write of one chunk. The view
writes the skeleton with a placeholder of the right size for each chunk.
`HttpServer` serves a chunk under a path of its view. `bring_offline` still
writes all of the content.

At level 3 the service keeps the decoded document between requests. It does
so already: an `HtmlService` holds its document for its whole life. What
level 3 adds is the engine work, so that a chunk costs in proportion to the
chunk and not to the document (#781 for ods, the issue #764 for the rest).

### 5. What level 2 changes for the reader, and the answer

| feature | level 2 changes | answer |
|---|---|---|
| `odr.search` | It searches the DOM. | It searches the chunk strings, and splices the chunk of a hit before it marks it. |
| Ctrl+F of the browser | It finds only what the browser lays out, at level 1 too. | `odr.search`, which a host shows as its find. |
| Editing a text file | An edit in a chunk that is then evicted is lost. | Eviction writes the chunk's DOM back to its string. `setContent` reads the strings. |
| Editing a sheet | A shown value lives in the DOM. | The ops are by position (`spreadsheet-editing.md` decision 1). A splice shows the values of the log again. |
| Print | It prints only what is in the DOM. | `beforeprint` puts every chunk back. Past a size, print refuses. |
| Selection and copy | A range over an evicted chunk loses its text. | Keep a margin of chunks around the viewport. |
| A link to an anchor | Its target may be in an evicted chunk. | The view states which chunk holds each anchor. |

### 6. A sheet is a table for each chunk of rows

`content-visibility` has no effect on a `tr` or a `tbody`, because size
containment does not apply to the inner boxes of a table. So a chunk of a
sheet is a table of its own:

```html
<div class="odr-sheet" data-odr-sheet="0" data-odr-sheet-name="…">
  <table class="odr-sheet-head">  <!-- colgroup, the ruler -->
  <table class="odr-sheet-rows odr-chunk odr-chunk-near"
         style="contain-intrinsic-height:auto 5120px">
    <colgroup>…</colgroup>
    <tbody><tr><th>1</th><td>…</td>…</tr>…</tbody>
  </table>
  <table class="odr-sheet-rows odr-chunk" …>…</table>
</div>
```

**The wrapper.** The attributes that `table.odr-sheet` states today
(`data-odr-sheet`, `data-odr-sheet-name`, `data-odr-cut`,
`--odr-print-fit`) go onto a `div.odr-sheet`. A rule that reads
`.odr-sheet td` stays as it is. A rule for the table itself moves to
`.odr-sheet-head` and `.odr-sheet-rows`.

**The ruler.** A sticky `thead` sticks only inside its own table, so the
column letters go into a table of their own before the chunks. That table
is sticky at the top of the wrapper. The row labels stay a `th` in each
row, and they stay sticky at the left, because each chunk is as wide as
the sheet.

**Column widths.** All of the tables must have the same columns, so each
table has `table-layout: fixed`, a `width`, and the same `colgroup`. A
column that states a width keeps it. Today a column that states no width
takes the width of its content, for example in a csv. Each chunk table
would then size it apart. So the writer gives such a column a width: the
longest text of its first 1000 rows, in `ch`, from 4 to 60. That is how
LibreOffice finds an optimal width. The cut of a longer text, and the
spill into the next cells, are the ones a stated width gets today.

**Where a chunk ends.** A chunk holds at most 256 rows, or as many rows as
hold 4096 cells, and at least 8 rows. A chunk ends only where no `rowspan`
is open, so no merge crosses two tables. A merge over very many rows makes
one large chunk.

**The height of a chunk.** The writer buffers the rows of a chunk, adds
their heights (the stated height of a row, else an estimate of 20px), and
states the sum as `contain-intrinsic-height: auto`. The body of a sheet is
already buffered (`spreadsheet_style_buffer`). A row that wraps its text is
taller than its estimate. The browser keeps the real height after the
first layout. Where a chunk above the viewport grows, scroll anchoring
holds the reader's place in a browser that has it. Safari may not have it,
so a reader there can see the page move by the error of the estimate.

**Shapes.** A shape is drawn from the cell A1 and can reach far down. So
the first chunk of a sheet with shapes is always near: the writer states
`data-odr-chunk-held`, and `viewport.js` does not observe that chunk.

**The scripts.**

| what | today | with chunks |
|---|---|---|
| The rows (`spreadsheet.js`) | `table.tBodies[0].rows` | the rows of every chunk, in order. One accessor replaces each read of `body.rows`. |
| The ruler | `table.tHead.rows[0]` | the row of `.odr-sheet-head` |
| Sort | moves the `tr`s inside one `tbody` | puts the sorted rows back into the chunks, each with as many rows as it had. A merged sheet has no sort, so no `rowspan` moves. |
| Insert and delete rows | inside one `tbody` | inside the chunk that holds the row. A chunk with no row left goes. |
| Insert and delete columns | every `col` and every row | the same, in every table |
| `reflow` (the spill) and the editor's overlay | read `offsetLeft` and `offsetTop` | A cell of a hidden chunk has no geometry. A `showValue` in a hidden chunk notes the row. `viewport.js` raises `odr-chunk-near` on a chunk when it becomes near, and `spreadsheet.js` then reflows the noted rows. |
| A selection over far rows | an overlay from cell to cell | The edge in a hidden chunk takes the edge of that chunk. Only the part in the viewport is seen, and it is exact. |
| The pin, and a move by keyboard | `scrollIntoView` of the cell | `odr.chunks.show(cell)` marks its chunk near first, as `odr.search` does. |
| Formulas in the page | read the text of a cell | no change: the DOM of a hidden chunk is still there |

**What stays.** The limits (`spreadsheet_limit`, `spreadsheet_cell_limit`)
stay as they are in this step. A sheet of 500 K cells takes 7.3 s to its
first frame today, and about 3 s of that is the scripts. Whether the
limits can go up is a measurement after this step.

## Order of work

1. **Text files, level 1.** Chunks of lines that are `hidden` until
   `text.js` marks them near the viewport. The line number moves into each
   line as a CSS counter, so the gutter needs no `ResizeObserver` and wraps
   with its line. A chunk has style containment, so a counter does not count
   on past it. The first line of each chunk sets the number it starts at.
2. **Text files, level 3.** The chunk source, the chunk write of core and
   the HTTP route. A text file needs no engine work, so it proves the API.
   Level 2 gains little for a text file: a window of strings painted in
   0.5 s with 1.2 GB, and the hidden chunks in 0.85 s with 0.9 GB.
3. **pdf, level 1.** A page has its size, so each page is a chunk of
   exactly the right size. The observer moves into `viewport.js`, which
   every view loads.
4. **Sheets, level 1** (decision 6). First the markup and the column
   widths in the writer, and the row accessor in `spreadsheet.js`. Then the
   geometry: `reflow`, the overlay, and the pin.
5. **Core per engine, level 3.** ods first (#781). The ods decode keeps
   `content.xml` and its DOM resident for editing. A render on demand that
   does not hold them is a decision of its own.

## Open questions

- **The size of a chunk.** 256 lines is a guess. It trades the count of
  chunks against the cost of one.
- **When a view takes level 2.** A threshold on the size of the content, or
  a switch in `HtmlConfig`.
- **Search at level 3.** The page holds no strings for content it never
  received. Search then needs the host.
- **The find of the browser.** `hidden="until-found"` lets it reach a hidden
  chunk and show it. But a browser without that value hides the chunk for
  good, where a browser without `content-visibility` shows it.
- **Eviction at level 3.** Whether the page drops a far chunk entirely and
  asks for it again, or keeps its string.
