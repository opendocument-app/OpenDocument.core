# Lazy loading design

Status: level 1 has landed for text files (#1247), pdf (#1248) and sheets
(#1250), and level 3 for text files, sheets and pdf. The rest is a
proposal. Issue #764.

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
| **3. On demand** | The page loads a chunk as a script when the reader comes near, and core renders only that chunk. | the HTTP server, which renders a chunk when it is asked, and a file that `bring_offline` writes, with every chunk beside it |

**Why levels:** level 1 is CSS and a few lines of script, and it keeps the
whole DOM. So `odr.search`, the editor and a copy of a selection see all of
the content. The find of the browser and a screen reader do not see a
hidden chunk. Level 2 removes the DOM cost too, but takes more away
(decision 5). Level 3 is the only level that makes core cheaper, where a
server renders a chunk only when the page asks for it. A view takes the
lowest level that is enough for the size of its file.

`HtmlConfig::lazy_layout` turns level 1 off. Then no view writes a chunk,
and the find of the browser and a screen reader reach all of the content.
The size of a chunk stays inside the views, because the measurements were
flat around it.

`content-visibility` is in Chrome from 85, Firefox from 125 and Safari from
18. A browser without it ignores the property and lays out every chunk, as
it lays out every line today.

**Offline and server mode:** every level works in a file that
`bring_offline` writes. At level 3 the file has its chunks beside it, so
the page stays small and loads only what the reader comes near. What only
a server adds is that core does not render or send the whole document at
the start.

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

### 3. A chunk that loads is a script of its own

```html
<div class="odr-text-lines odr-chunk" data-odr-chunk="12"
     data-odr-chunk-src="text-chunks/12.js"></div>
```

```js
// text-chunks/12.js
odr.chunks.fill(12, "<div style=\"counter-reset:odr-line 3072\">…</div>…");
```

When `viewport.js` marks such a chunk near, it adds a `<script>` with the
source, and the script puts the lines in through `odr.chunks.fill`. Until
then the chunk holds the height of its lines (`min-height`), so the page
does not move when they come.

**Why a script, not `fetch`:** a script loads over HTTP and from a file
alike, where `fetch` of a `file://` URL is blocked. So one page works in
both places, and a binding that renders into a web view from files needs
no callback of its own. The web view must let the page read the files
beside it, as it already must for the linked css and js.

**What stays in the page:** the first chunk, so the first frame needs no
request. An editable render keeps every line in the page, because its log
states the whole text. A page that the browser does not read as UTF-8
keeps them too, because a script is UTF-8.

### 4. Core writes a chunk as a resource of its view

The view returns each chunk as an `HtmlResource` whose `write_resource`
renders that chunk on demand. `bring_offline` writes them beside the page,
and `HttpServer` serves them through `HtmlService::exists` and `write`,
with no route of its own. `HtmlConfig::lazy_loading` turns it on, and it
needs `lazy_layout`.

The service keeps the decoded document between requests. It does so
already: an `HtmlService` holds its document for its whole life. The text
service also keeps the text and where each chunk starts in it, so a chunk
costs in proportion to the chunk. For the other engines, level 3 needs the
engine work that makes a chunk cost in proportion to the chunk and not to
the document (#781 for ods, the issue #764 for the rest).

**Measured:** the text file of one million lines is a page of 485 KB and
3906 scripts of about 24 KB. Over `file://` it paints its first frame
after 67 ms (level 1: 0.85 s). The HTTP server answers the page in 4 ms
and a chunk in 2 ms.

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

**Column widths.** All of the tables must have the same columns. So each
table has `table-layout: fixed` and the same `col`s, and states the width
of the whole sheet (`--odr-sheet-width`, in px). Without that width, a
table wider than the viewport shrinks each column to what its own cells
hold, and the chunks do not line up. A column that states a width keeps
it. A column that states no width, for example in a csv, took the width of
its content before. Now the writer estimates it from the longest text of
its first 1000 rows, from 4 to 60 characters, at 7.5px for each character
and 14px of padding. A cell that wraps its text counts only its longest
word. The row labels get 7.5px for each digit and 14px. The estimate does
not know the font of the file, so a wide font can spill a long number past
its column.

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
| `reflow` (the spill) and the editor's overlay | read `offsetLeft` and `offsetTop` | A cell of a hidden chunk has no geometry. A `showValue` in a hidden chunk notes the row. `viewport.js` dispatches an event on a chunk when it becomes near, and `spreadsheet.js` then reflows the noted rows. |
| A selection over far rows | an overlay from cell to cell | The edge in a hidden chunk takes the edge of that chunk. Only the part in the viewport is seen, and it is exact. |
| The pin, and a move by keyboard | `scrollIntoView` of the cell | `odr.chunks.show(cell)` marks its chunk near first, as `odr.search` does. |
| Formulas in the page | read the text of a cell | no change: the DOM of a hidden chunk is still there |

**What stays.** The limits (`spreadsheet_limit`, `spreadsheet_cell_limit`)
stay as they are in this step. A sheet of 500 K cells
(`efficiency-big-1.ods`) took 7.0 s to its first frame, and takes 2.2 s
with chunks. The same page without its scripts takes 2.0 s. Before, the
page with chunks took 4.0 s, and 1.8 s of that was `viewport.js`. To find
whether the browser adjusts the text size, it set the zoom of the body
twice, and each set made the browser compute the style and the layout of
the whole document again. It now probes in an `iframe` of its own. Whether
the limits can go up is the next measurement.

### 7. A pdf page at level 3 renders on its own

Measured on `Core_v5.1.pdf` (2985 pages), 2026-10-24:

| step | time | memory |
|---|---|---|
| core: parse the pdf | 0.5 s | |
| core: render the whole-document view | 4.9 s | 761 MB peak |
| core: render one page view | 5 to 37 ms | |
| browser: first frame at level 1 (#1248) | 1.8 s, nearly all of it parsing the html | 2.0 GB |
| the page: `document.html` | 127 MB: 5.4 MB of head (2.3 MB of it 89 fonts), 122 MB of pages | |

So for a server, the time is in core. The page is ready after 5.4 s, and
one page would be ready after about 40 ms. For a file that `bring_offline`
wrote, core works once, and the reader waits for the browser.

The writer renders a set of pages in one pass
(`write_pages_dual_layer`, `write_pages_single_layer`). The pass names its
classes and fonts as it meets them (`c0`, `odr-f1`). In `single_layer`, it
also re-encodes each font with the code points of the runs it meets. Then
it writes the head and the pages. A page that loads later has to bring what
the head lacks.

**Option A, the page side.** As the sheets do (decision 6, order of work
item 5): the pass renders every page as it does now, so the head has every
class and font. The pages after the first become scripts. A script renders
its page again with the registries the pass kept.

- The page is about 5.5 MB, not 127 MB. The browser parses and holds about
  one page in 25 of what it does now.
- Core still renders everything before the page goes out: 4.9 s and
  761 MB for this file. The service keeps the registries for as long as it
  lives.
- A small change to the writer: the pass and the render of one page share
  the registries.

**Option B, a page on its own,** is what was built. The page renders the
first page only. A script renders its page alone.

- Core work is in proportion to what the reader sees.
- **What a script brings:** a `<style>` with the rules its page needs, and
  what the page box holds. The empty page in the document is already the
  page box: it states its size, its print page, its id and its
  `data-odr-page`, so `odr.chunks.fill` puts the content in as it does for
  a text file.
- **Classes:** a page rendered alone suffixes its names with its number
  (`x0_12`, `odr-s1_12`), so two pages cannot clash. The ids of its SVG defs
  take its number too. Every script states the `.fvN` rule of its fonts
  again, and the rules of a later script come later in the document. So a
  page's own rules state their class three times (`.k0_12.k0_12.k0_12`) and
  rank above that single class.
- **Fonts:** the dual layer re-encodes a font to the PUA range only, so its
  program is the same for every page. The document lists the fonts that the
  pages and their forms state in their resources, numbers each one for the
  whole service, and links it as `pdf-fonts/<n>.ttf`. A font that no
  resources state, for example one an annotation draws with, stays inline
  in the page that uses it.
- **Images:** a script carries its images inline, because nobody writes
  the resources of a script.
- **Where it stays inline:** the single layer re-encodes a font with the
  code points of the runs it meets, so the font of a page alone would
  differ. That mode keeps every page in the document.

**Measured** on `Core_v5.1.pdf`:

| | before | at level 3 |
|---|---|---|
| the document, over HTTP | 5.4 s to the first byte | 0.5 s |
| a page script, over HTTP | | 3 to 6 ms |
| a font file, over HTTP | | under 1 ms |
| `document.html` | 127 MB | 1.4 MB, with 2984 scripts and 84 fonts (1.8 MB) beside it |
| core memory to write the document | 761 MB | 138 MB |
| first frame from `file://` | 1.8 s | 92 ms |

At the start, in the middle and at nine tenths of the document, a
screenshot is the same to the pixel as one of the page with every page in
it.

### 8. A search finds the chunks that have not loaded

At level 3, `odr.search` searches the DOM, so it finds only the chunks that
have loaded. To load every chunk for a search would bring back the cost
that level 3 removes: 127 MB of html for `Core_v5.1.pdf`. A search on the
server works only over HTTP. So the page searches a text index that core
writes beside the chunks, and loads the chunk of a hit only when the reader
goes to it.

**Measured:** the plain text of `Core_v5.1.pdf` is at most 9.0 MB, and
1.3 MB gzipped (ghostscript `txtwrite`, which pads the layout with
spaces). The html of its pages is 122 MB.

**The index is a script, as a chunk is.** One script holds the text of 64
chunks, one string for each chunk:

```js
// text-search/0.js
odr.chunks.text({"1": "the text of chunk 1", "2": "the text of chunk 2", …});
```

The first chunk of a group that loads names the index script of the
group:

```html
<div class="odr-text-lines odr-chunk" data-odr-chunk="64"
     data-odr-chunk-src="text-chunks/64.js"
     data-odr-chunk-text="text-search/1.js"></div>
```

The keys are the keys of `data-odr-chunk`, which `odr.chunks.fill` takes
too. A script holds only the chunks that load, not the first. The paths follow
the paths of the chunks: `text-search/<n>.js`, `sheet<n>-search/<m>.js`,
`pdf-search/<n>.js`. The view returns each script as an `HtmlResource`, so
`bring_offline` writes it and `HttpServer` serves it with no new route
(decision 4). An attribute on every chunk made the page of a million lines
a third larger (648 KB, not 485 KB). On the first chunk of each group, it
costs 3 KB.

**What the text of a chunk is.** The text that `search.js` reads from the
DOM of that chunk: every text node outside `script`, `style` and
`aria-hidden`, in document order. A `\n` separates two blocks, because
`search.js` never matches across a block boundary. `search.js` folds a `\n`
inside a block to a space, as it folds a tab. So the separator is the only
`\n` that the folded text keeps, and a keyword never matches across it.

| view | a block |
|---|---|
| text file | a line |
| sheet | a cell; the headers of the rows and the columns are not in the index |
| pdf | a line of the `.sel` layer; `.vis` is `aria-hidden` |

**A search does not find the headers of a sheet.** Today a search of the
DOM finds the row numbers and the column letters, so "12" finds row 12.
They are labels, not content. So `search.js` skips the cells of
`odr-sheet-row-header` and `odr-sheet-column-header`, at every level, and
the index of a sheet does not hold them. The markup does not change.

Core writes the index from the same model that it writes the chunk from:
the lines, the cells, the `SelLineOut`s. It does not write it from the
html. The cost in core:

- **Text file:** the service already keeps the text and where each chunk
  starts (decision 4). The index is the text.
- **Sheet:** the text of each cell. The decode is resident, so this costs
  less than the render of the rows.
- **pdf:** the text runs of every page. This interprets every content
  stream, but writes no html, no fonts and no images. The service keeps the
  result, so it does this once.

**How the page searches.**

1. `odr.search(keyword)` marks the hits in the loaded chunks, as it does
   now.
2. It then loads the index scripts once, with a `<script src>` tag, and
   keeps the strings.
3. It counts the hits in each chunk that has not loaded. Such a hit is a
   chunk and an ordinal in that chunk, with no mark.
4. When the reader steps to such a hit, `odr.chunks.show` loads its chunk.
   On `odr-chunk-filled`, `search.js` marks the hits of that chunk in the
   DOM and selects the hit.
5. A chunk that loads because the reader scrolls to it gets its marks the
   same way.
6. When the DOM count of a chunk differs from its index count, the DOM
   count replaces it. This is the safety net, and a test asserts that the
   two counts are the same for each fixture.

`fold` must be fast on a large index: an index of a million lines is the
whole text file, and `fold` normalizes one character at a time. So the
text of a chunk folds with no map: a character that is not ASCII folds on
its own, through a cache, and then one `toLowerCase` folds the rest.

**Measured** on the text file of one million lines (71 MB), from `file://`:
the index is 62 scripts of 69 MB in all. A search loads them and counts
every hit in 175 ms, and the counts are those of the file (120, 33407 and
566808 hits). A search with the index loaded takes 155 ms. A step to the
last hit loads its chunk and selects the hit in 9 ms.

**Measured** on the sheet of 500 K cells in `efficiency-big-1.ods`: the
index is 2 scripts of 68 KB, beside 100 MB of chunks. For four keywords, the
count from the index is the count of a page with every row in it, and the
search takes 6 to 13 ms there, against 205 to 398 ms in that page.

**Measured** on `Core_v5.1.pdf` (2985 pages): the index is 47 scripts of
5.3 MB. Core renders each page alone and to nothing for its text, 4.9 s for
all of them, and keeps the text. For five keywords, the count from the
index is the count of the page with every page in it (7387 hits of
"Bluetooth", 461030 of "é"). The search takes 12 to 54 ms, against 0.7 to
9.4 s in that page. A step to the last hit loads page 2985 in 24 ms.

**Over HTTP, the server searches.** The index then stays on the server,
and the page does not load 9 MB of text for one keyword:

```js
// pdf-search/hits/protocol%20stack.js
odr.chunks.hits("protocol stack", {"p1": 2, "p18": 1, "p2211": 5});
```

1. The page folds the keyword and asks for `<index directory>/hits/<the
   folded keyword>.js`, with a `<script src>` tag, as for the other scripts.
   It takes the index directory from `data-odr-chunk-text`.
2. `HtmlService::exists` and `write` answer that path, so `HttpServer`
   needs no new route. The keyword is the part between `hits/` and the
   last `.js`, so a keyword with a `/` works too.
3. The service searches the index that it keeps for the index scripts,
   and answers each chunk that has a hit, with its count.
4. The page does this only where `location.protocol` is `http:` or
   `https:`. If the script fails, it loads the index scripts. That is the
   case for a file that `bring_offline` wrote and a static server serves,
   because `bring_offline` writes no `hits/` path.

The server must fold as `search.js` folds, or the counts differ. `fold`
folds one UTF-16 unit at a time, so a table states it exactly: for each
unit whose fold is not the unit itself, the units it folds to. A script
generates that table from the `fold` of `search.js` with node, so the two
cannot differ, and core needs no Unicode library. A test runs the C++ fold
and the JS fold over the same strings. Where the counts still differ, the
DOM count of a loaded chunk replaces the server count, as it replaces the
index count.

**The API for a host.** A search at level 3 completes only after the index
scripts load, so it is asynchronous:

- `odr.search` returns the count that it knows now, as it does today. A page
  that does not load chunks knows every hit at once, so for that page
  nothing changes.
- `odr.onSearchChange({count, current, complete})` reports a new count or a
  new current hit. `host-bridge.js` forwards it as `searchChange`, as it
  forwards the other `odr.on*` callbacks. A callback, and not a Promise,
  because the Android `evaluateJavascript` does not wait for a Promise.
- `odr.searchDone()` returns a Promise of the final count, for a host that
  can wait for one. The ios app calls `odr.search` with
  `callAsyncJavaScript`, so its screenshot of a search can wait for it.

**The apps.** The ios app calls `odr.search`, and needs only the new
callback to show the count. The droid app uses the find of the WebView
(`findAllAsync`). That find does not reach a chunk that is
`content-visibility: hidden`, so it already misses text at level 1. The
droid app must move to `odr.search` and `searchChange`. A core PR names
that issue with `Refs`.

## Order of work

1. **Text files, level 1.** Chunks of lines that are `hidden` until
   `text.js` marks them near the viewport. The line number moves into each
   line as a CSS counter, so the gutter needs no `ResizeObserver` and wraps
   with its line. A chunk has style containment, so a counter does not count
   on past it. The first line of each chunk sets the number it starts at.
2. **Text files, level 3** (decisions 3 and 4). Level 2 gains little for
   a text file: a window of strings painted in 0.5 s with 1.2 GB, and the
   hidden chunks in 0.85 s with 0.9 GB.
3. **pdf, level 1.** A page has its size, so each page is a chunk of
   exactly the right size. The observer moves into `viewport.js`, which
   every view loads.
4. **Sheets, level 1** (decision 6). First the markup and the column
   widths in the writer, and the row accessor in `spreadsheet.js`. Then the
   geometry: `reflow`, the overlay, and the pin.
5. **Sheets, level 3, the page.** A chunk of rows after the first is an
   empty table that names `sheet<n>-chunks/<m>.js`, on the decode as it is.
   The view writes the rows of such a chunk to nothing, because that is
   where it finds the chunk to end, and renders them again, with inline
   styles, when the script is asked for. A sheet whose rows load has no
   sort control, because a sort needs every row. The sheet of 500 K cells
   in `efficiency-big-1.ods` is a page of 370 KB that paints after 107 ms.
6. **pdf, level 3** (decision 7, option B).
7. **Core per engine, level 3.** ods first (#781). The ods decode keeps
   `content.xml` and its DOM resident for editing. A render on demand that
   does not hold them is a decision of its own. It waits for a measurement
   of the page side.
8. **Search at level 3** (decision 8), in this order:
   1. Text files, the index scripts. This PR also brings the fast path of
      `fold`, `odr.onSearchChange` and `odr.searchDone`.
   2. Text files, the search on the server. This PR brings the generated
      fold table, its test, and the `hits/` path.
   3. Sheets: no search of the headers, at every level, and then the index
      and the `hits/` path.
   4. pdf: the index and the `hits/` path.

## Open questions

- **Print at level 3.** A chunk that has not loaded prints as empty space
  of its height. `beforeprint` cannot wait for a script to load.
- **Fragments over HTTP.** A chunk is a script, because only a script loads
  from `file://`. Over HTTP, the server could also serve a chunk as a plain
  HTML fragment (`text-chunks/12.html`), and `viewport.js` could `fetch` it
  when the page is not on `file://`. The script stays the fallback. Only
  `write_chunk` and `load()` know the format.
- **A pdf in the single layer at level 3.** The single layer votes over
  the runs of every page in its pass for the code point of each glyph, and
  re-encodes the font with the winners. A page rendered alone would vote
  differently and need a font of its own. Two ways:
  - Each script carries its fonts inline, with suffixed face names
    (`odr-f3_12`). This is about 0.5 MB for a page of `Core_v5.1.pdf`, and
    the browser loads a font again for each page.
  - The service votes once over the whole document and keeps the winners,
    so every page links one file for each font. This costs a pass over
    every page before the first byte.
- **The size of a chunk.** 256 lines is a guess. It trades the count of
  chunks against the cost of one.
- **When a view takes level 2.** A threshold on the size of the content, or
  a switch in `HtmlConfig`.
- **The find of the browser.** `hidden="until-found"` lets it reach a hidden
  chunk and show it. But a browser without that value hides the chunk for
  good, where a browser without `content-visibility` shows it.
- **Eviction at level 3.** Whether the page drops a far chunk entirely and
  asks for it again, or keeps its string.
