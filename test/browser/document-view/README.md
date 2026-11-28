# source/view editing checks

These check the text editor through `document-view.js` with a projection
connected, which the text checks never do.

```bash
test/browser/document-view/serve    # serves on :8756
PLAYWRIGHT_MODULE=/path/to/node_modules/playwright \
  node test/browser/document-view/check.cjs
```

Unlike the other directories, this is a script and not a page: native typing
and IME composition need trusted input, which only a browser driver can send
(`Input.imeSetComposition` over CDP). `BROWSER=firefox` runs every check but
composition in Firefox; `BROWSER_EXECUTABLE` picks a browser binary.

`fixture.html` splits each paragraph into fragments at a fixed UTF-16 offset,
so the checks pin the editor/adapter contract without a paginator. Each check
compares the source run's full text, the projected text and the exported
operations: typing, directional selection, Unicode deletion, paragraph split
and merge, formatting, undo/redo, and composition at paragraph edges and in
empty runs and paragraphs.
