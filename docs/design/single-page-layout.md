# Single-page layout design

Status: built.

Related: [`pagination.md`](pagination.md) describes progressive text pages and
their provisional count. Single-page text navigation remains unsupported.

[`lazy-loading.md`](lazy-loading.md) holds the chunks a page is
laid out in. [`structure-editing.md`](structure-editing.md) holds the slide
gestures, which have to keep the current page in view.

## Problem

A view of a workbook, a presentation, a drawing or a pdf states every sheet
or page in one column, and the reader scrolls. A presentation is read one
slide at a time, a workbook one sheet at a time, and a pdf on a phone one
page at a time. Today a host that wants that has two options, and neither
works well:

- **One view per page** (`page0.html`, ...). Switching pages loads another
  document, so the page loses what it holds: the edit log and its undo, a
  search, a selection, the zoom.
- **Scrolling the column itself.** The host has to find the pages in the
  markup, and every host does it again.

There is also no way to address a page from outside. A link cannot open the
third slide.

## Decisions

### 1. A layout in `HtmlConfig`

```cpp
/// How a view lays out its pages.
enum class HtmlPageLayout {
  continuous, ///< every page in one column, as now
  single,     ///< one page shown at a time; the script switches
};

struct HtmlConfig {
  // ...
  HtmlPageLayout page_layout{HtmlPageLayout::continuous};
};
```

- An enum, not a `bool`: a spread of two pages is the next layout, and it
  then needs no second field. A new value goes at the end, because the
  bindings map the enum by ordinal.
- It applies to the sheets of a spreadsheet, slides, drawing pages and pdf
  pages. A view of one sheet or one slide has one page, so it changes
  nothing there. A text document is read as a column, page boxes or not, so
  it ignores the layout; so does every view without pages.
- `continuous` is the default, so nothing changes for a host that does not
  ask.

### 2. The markup stays the same

The view writes every page as now. Three kinds of page are written, each
marked its own way:

- a slide or a drawing page: `.odr-pages > .odr-page-outer`, with
  `data-odr-page` (an element id) only when the render is editable;
- a sheet: `body > .odr-sheet`, with `data-odr-sheet` (its ordinal);
- a pdf page: `div.d > div[data-odr-page]`, with `id="pN"` (1-based) and
  `data-odr-page` (the 0-based index).

So each also gets the class `odr-page`, in both layouts: the one hook the
CSS and the script need. Under `single`, `<body>` carries
`data-odr-page-layout="single"`, and `write_page_layout_style` writes one
rule:

```css
@media screen{[data-odr-page-current] .odr-page:not(.odr-page-current){display:none}}
```

- The rule waits for the script: `viewport.js` marks the current page and
  sets `data-odr-page-current` on `<body>`. Until it runs, and without it,
  every page shows and the view reads as `continuous`. The first page is on
  top in that column anyway, so the first frame shows the page the script
  then keeps, unless `#page=N` names another one. No `<noscript>` rule and
  no current page in the markup are needed.
- A print shows every page, as `continuous` does: the rule is screen only.
- Lazy loading keeps working. The current page and its two neighbours are
  near, so a page is laid out before it is shown. At level 3, a page whose
  chunk has not loaded is loaded when it becomes current, and shown once it
  has.

### 3. The script API

In `viewport.js`, next to the zoom, and in both layouts:

| Call | What it does |
|------|--------------|
| `odr.getPageCount()` | the number of pages |
| `odr.getPage()` | the 0-based index of the current page: the shown one in `single`, the one at the top of the viewport in `continuous` |
| `odr.setPage(index)` | shows the page in `single` and scrolls to it in `continuous`; false for an index past the last |
| `odr.onPageChange({page, count})` | after the current page changed, for whatever reason |

- The index is 0-based, as `odr.editing.insertTextBox({page})` takes it, and
  is the page's position among the `.odr-page` elements. It equals the pdf
  view's `data-odr-page` and a sheet's `data-odr-sheet`, but not a slide's
  `data-odr-page`, which holds an element id.
- `host-bridge.js` forwards `onPageChange` as `pageChange`, as it does every
  `odr.on*` callback, so an app shell builds its own controls.
- In `single`, the arrow keys, PageUp, PageDown, Home and End switch pages
  while the focus is not in something that takes the keys: an edit, a form
  field.

### 4. The URL states the page

The page reads `#page=N` and keeps it current.

- **1-based**, as the pdf viewers of the browsers read `#page=`. A link to
  the third slide is `slides.html#page=3`.
- **`#page=N`, not the pdf view's `#pN` anchors.** Those ids only scroll,
  only in the pdf view, and a hidden page cannot be scrolled to. The anchors
  stay as they are.
- **A fragment, not a query.** It never reaches a server, so it works under
  `file:`, `blob:` and `HttpServer` alike, and the cached page is the same
  for every page.
- **Read at load and on `hashchange`.** A host chooses the page by the URL
  it opens, or by `odr.setPage`.
- **Written with `history.replaceState`**, so turning pages does not fill
  the back stack. A host that wants each page in the history listens to
  `onPageChange` and pushes the entry itself.
- **In both layouts.** In `continuous`, the fragment scrolls to the page
  once at load, and the page writes it as the reader scrolls.
- A number past the last page, or not a number, opens at the first page and
  leaves the fragment alone.

No `HtmlConfig` field states a first page. The fragment does that, and a
host that renders once for every reader cannot know it.

### 5. What has to follow the current page

- **Search.** A hit on another page makes that page current before the hit
  scrolls into view. `search.js` already calls `odr.chunks.show` on a hit,
  and under `single` that turns to the hit's page, so search needs no change.
- **Selection.** A selection never spans a hidden page, because a hidden
  page cannot be selected.
- **Structure edits.** An inserted slide becomes current, as it scrolls into
  view now. Removing the current slide makes its neighbour current, the next
  one, else the previous one. A moved slide stays current. `viewport.js`
  holds the current page as the element and watches the page column, so an
  undo or a redo of any of these lands the same way.
- **Pdf annotations.** A highlight is drawn on the current page; the pages
  that are hidden keep theirs, as a far chunk does now.
- **Fitting the width.** `fit_width_by_view` measures the current page, so
  turning to a page of another size refits it. A pdf can mix page sizes.

## Open questions

- A spread of two pages: whether it gets a layout of its own now, or waits
  for a host to ask.
- Whether `single` animates the switch. A host can do it through
  `onPageChange`, so the view does not need to.
- Whether `odr.getPage` in `continuous` should report the page at the top of
  the viewport or the one that covers most of it. The top is simpler and is
  what a pdf viewer reports.
