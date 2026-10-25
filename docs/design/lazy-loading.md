# Lazy loading design

Status: levels 1 and 3 have landed for text files, sheets and pdf, level 1
for presentations and drawings, the search over parts that have not
loaded, and images that load near the viewport. Three items are left (see
"Left"). Issue #764.

Related: [`editing.md`](editing.md) holds the editing mode, which a page that
drops content must keep working. [`txt-editing.md`](txt-editing.md) holds the
editor of the text file view.

## Problem

A large file fails in two places, and the browser fails first.

- **The browser.** A page that states every line, row or page as DOM costs
  layout and memory for content the reader never sees. A text file of one
  million lines painted its first frame in Chrome after 154 s, with 6.3 GB
  in the renderer.
- **Core.** A decode holds the whole document. `efficiency-big-1.ods` (297 MB
  of `content.xml`) peaks at 697 MB in `translate`: 423 MB for the XML and
  its DOM, 230 MB for the registry and the styles.

Layout is the cost, not the DOM alone. Chunks of 256 lines that stay
`content-visibility: hidden` until a script marks them near paint the
million lines after 0.85 s with every node kept. `content-visibility: auto`
is not enough: Chrome lays out each `auto` chunk once on the first frame to
find out whether it is near (13.5 s).

## Decisions

### 1. Three levels, and each one works without the next

| level | what it does | switch |
|---|---|---|
| **1. Containment** | A view writes its content in chunks. A chunk is `content-visibility: hidden` until `viewport.js` marks it `odr-chunk-near`, so the browser lays out only those. | `HtmlConfig::lazy_layout`, on by default |
| **2. A window** | The page keeps far chunks as strings, not DOM. | not built: levels 1 and 3 were enough |
| **3. On demand** | A chunk after the first is a script beside the page, which the page loads when the reader comes near. Core renders only that chunk. | `HtmlConfig::lazy_loading`, off by default |

- Every level works offline: `bring_offline` writes the scripts beside the
  page, and `HttpServer` serves them through `HtmlService::exists` and
  `write`, with no route of its own.
- The first chunk is near in the markup, so the first frame needs no script.
  `data-odr-chunk-held` keeps a chunk near. Without a script, a
  `<noscript>` rule gives the chunks `auto`. A print shows every chunk that
  is in the DOM, so at level 3 not one that has not loaded (see "Open
  questions").
- A browser without `content-visibility` lays out every chunk, as before.
- An editable render and a page that is not UTF-8 stay at level 1, because
  the edit log states the whole content, and a script is UTF-8.

### 2. A chunk is a script, and core renders it on demand

```html
<div class="odr-text-lines odr-chunk" data-odr-chunk="64"
     data-odr-chunk-src="text-chunks/64.js"
     data-odr-chunk-text="text-search/1.js"></div>
```

```js
// text-chunks/64.js
odr.chunks.fill(64, "<div style=\"counter-reset:odr-line 16384\">…</div>…");
```

- A script loads from `file://` and over HTTP alike, where `fetch` of a
  `file://` URL is blocked. So one page works in both places.
- Until it fills, a chunk holds the height it will take, so the page does
  not move. A chunk fires `odr-chunk-near` and `odr-chunk-filled`.
- The view returns each script as an `HtmlResource` whose `write_resource`
  renders the chunk then. The service keeps the decode between requests.

### 3. What a chunk is in each view

| view | chunk | level 3 |
|---|---|---|
| text file | 256 lines. The line number is a CSS counter in each line; the first line of a chunk resets it, because a chunk has style containment. | `text-chunks/<n>.js`. The service keeps the text and where each chunk starts. |
| sheet | a `table.odr-sheet-rows` of about 4096 cells, which ends only where no rowspan is open, because `content-visibility` has no effect on a `tr` or a `tbody`. A `table.odr-sheet-head` holds the ruler, and every table states the same column widths in px. | `sheet<n>-chunks/<m>.js`. The page render writes the rows of a later chunk to nothing, to find where the chunk ends. Such a sheet has no sort control. |
| pdf | a page | `pdf-pages/<n>.js` renders its page alone. It suffixes its classes and def ids with its number (`x0_12`), and its own rules outrank a single class (`Rank::replaces_inline`), so a later page cannot override an earlier one. The dual layer encodes a font the same way for every page, so the document links each font once as `pdf-fonts/<n>.ttf`. Images are inline in the script. The single layer encodes a font for the runs it meets, so it keeps every page in the document. |
| presentation, drawing | a slide or a page, in the view of the whole document. A view of one slide has no chunk. | not built: decks are small once their images are files (decision 5) |
| text document | none (decision 6) | none |

### 4. The search finds the parts that have not loaded

At level 3, the DOM holds only the loaded chunks, and loading every chunk
for a search would bring back the cost. So core writes a text index beside
the chunks, and the page loads the chunk of a hit when the reader goes to
it.

```js
// text-search/1.js: the text of 64 chunks, keyed as data-odr-chunk
odr.chunks.text({"64": "line 16385\nline 16386", …});
```

- **The text of a chunk** is what `search.js` reads from its DOM, with a
  `\n` between two blocks, because a search never matches across a block.
  `search.js` folds a `\n` inside a block to a space. A block is a line
  (text file), a cell or a paragraph in it (sheet), or a line of the `.sel`
  layer (pdf). Core takes it from the model, not from the html: the lines,
  the cells (`BlockText`), and the `.sel` lines that the dual layer returns
  (`DualLayerPages`). A pdf renders each page once, to nothing, for it, and
  the service keeps the text.
- **The first chunk of a group** names the index script. An attribute on
  every chunk made a page of a million lines a third larger.
- **The page:** `odr.search` marks the hits in the loaded chunks and
  returns that count. It then loads the index once and counts the other
  chunks; such a hit is a chunk and an ordinal. A step to it loads the
  chunk, and `odr-chunk-filled` marks and selects it. Where the DOM count of
  a chunk differs from the index count, the DOM count wins.
- **The API:** `odr.onSearchChange({count, current, complete})`, which
  `host-bridge.js` forwards as `searchChange`, and `odr.searchDone()`, a
  Promise of the final count. A callback, because the Android
  `evaluateJavascript` does not wait for a Promise.
- `fold` folds a non-ASCII character on its own, through a cache, and the
  rest with one `toLowerCase`, so an index of 69 MB folds in about 150 ms.
- A search does not find the row numbers and the column letters of a sheet,
  at every level.

### 5. An image is a file that loads near the viewport

The large office outputs are 90 to 100 % data URLs, with 0.7 K to 37 K
elements, so a chunk would gain little there. With `embed_images` off,
which the apps set:

- An `<img>` states `loading="lazy"` and `decoding="async"`.
- A linked image holds what its data URL holds: an SVM is written as SVG at
  its path plus `.svg`, and every image states its media type. A conversion
  that fails writes nothing and warns.

### 6. A text document waits for a measurement

It gets lazy loading only when a file's DOM, not its images, makes the
first frame slow. No file in the corpus does; the largest has 37 K
elements. Paged (`text_document_margin`), each page box would be a chunk,
as a slide is. Reflowed, a chunk would be a run of top-level blocks with an
estimated height (`contain-intrinsic-size: auto`). Level 3 would need the
chunk of each anchor, and the decode reads the whole XML anyway, so it
would make only the browser cheaper.

## Measured

Headless Chrome, `file://`, 2026-10-24 and 25.

| file | before | after |
|---|---|---|
| text file, 1 M lines | first frame 154 s | 0.85 s at level 1; 67 ms at level 3, with a page of 495 KB |
| `efficiency-big-1.ods`, 500 K cells | first frame 7.0 s | 2.2 s at level 1; 107 ms at level 3, with a page of 370 KB |
| `Core_v5.1.pdf`, 2985 pages | first frame 12 s; a document of 127 MB, 5.4 s to the first byte over HTTP, 761 MB in core | 1.8 s at level 1; 92 ms at level 3, with 1.4 MB, 0.5 s and 138 MB |
| a deck of 600 slides, 426 K elements | first frame 570 ms | 150 ms |
| `physics.docx`, 187 images | 187 images at the start | 3, with linked images |

The search at level 3 counts the same hits as a page with everything in it:

| file | index | search |
|---|---|---|
| text file, 1 M lines | 62 scripts, 69 MB | 175 ms, then 155 ms |
| `efficiency-big-1.ods` | 2 scripts, 68 KB | 6 to 13 ms (205 to 398 ms in the full page) |
| `Core_v5.1.pdf` | 47 scripts, 5.3 MB, 4.9 s in core | 12 to 54 ms (0.7 to 9.4 s in the full page) |

A step to the last hit loads its chunk and selects the hit in 9 to 24 ms.
A print to pdf of the chunked deck and of the linked images is the same as
without them.

## Not verified

- **WebKit.** The print of a lazy image and of a chunk that is not near,
  and the zoom probe of `viewport.js`, were checked in Chrome only. The ios
  app renders in WebKit.
- **The apps.** The droid app uses the find of the WebView, which does not
  reach a hidden chunk (opendocument-app/OpenDocument.droid#732). The ios
  app calls `odr.search`, and can show the count through `searchChange`.

## Left

1. **The search on the server.** Over HTTP, the first search of
   `Core_v5.1.pdf` waits about 5 s for its index. The server answers
   `<index directory>/hits/<folded keyword>.js` with a count for each chunk,
   from the index it keeps. A table generated from the `fold` of
   `search.js` makes the C++ fold the same. If that script fails, the page
   loads the index scripts.
2. **Core at level 3 for ods** (#781). The ods decode keeps `content.xml`
   and its DOM resident for editing; a render on demand that does not hold
   them is a decision of its own.
3. **Text documents** (decision 6), once a measured file asks for it.

## Known limits

- **Sheets.** A merge that stays open, such as a sidebar merged over the
  whole sheet, ends no chunk, so the sheet is one chunk. The page render
  still renders the rows of a later chunk to find its end, so a sheet costs
  core as much as before. A copy of a whole column or of the sheet holds
  only the rows that have loaded.
- **The index and the DOM.** A list marker in a sheet cell and a U+0000 in
  a text file are in one and not in the other. The DOM count of a chunk
  wins once it loads.
- **pdf page scripts.** A page script carries its images inline, so the
  browser cannot cache an image that repeats from page to page. A font that
  only a pattern, a soft mask, a Type 3 font or an annotation uses has no
  shared file, so each page that uses it carries it. Each script states the
  faces and the `.fvN` rules of its fonts again, which is why its rules
  outrank a single class; stating them once in the head would let the
  registry stay plain. The numbers of the shared fonts follow the order of
  an `unordered_map`, so they may differ between standard libraries.
- **Resources.** A chunk resource holds a bare pointer to its service, so
  it must not outlive the service.

## Open questions

- **Print at level 3.** A chunk that has not loaded prints as empty space
  of its height. `beforeprint` cannot wait for a script to load.
- **A pdf in the single layer at level 3.** The single layer votes over
  the runs it meets for the code point of each glyph, and re-encodes the
  font with the winners, so a page rendered alone has fonts of its own.
  That works: each page script re-encodes its fonts for its own runs and
  carries them with suffixed face names, as a page view (`pageN.html`)
  already does today. On `Core_v5.1.pdf` such a page carries about 300 KB of
  fonts, as much as in the dual layer, because neither subsets. The cost is
  that the browser loads a font again for each page. Two ways to cut it:
  name each re-encoded font file by a digest of its bytes, so the pages
  whose votes agree share one file, as the images of a pdf do; and subset a
  font to the glyphs of its page.
- **Eviction at level 3.** Whether the page drops a far chunk and asks for
  it again, or keeps its string.
- **The find of the browser.** `hidden="until-found"` lets it reach a hidden
  chunk, but a browser without that value hides the chunk for good.
- **Fragments over HTTP.** The server could serve a chunk as an HTML
  fragment for `fetch`, with the script as the fallback.
- **The size of a chunk.** 256 lines is a guess.
