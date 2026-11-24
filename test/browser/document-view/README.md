# Source/view editing checks

Run `test/browser/document-view/serve`, then `node test/browser/document-view/check.cjs`.
Set `PLAYWRIGHT_MODULE` to an installed Playwright module, `BROWSER_EXECUTABLE` to
the browser path, and `BROWSER=firefox` to use Firefox instead of Chromium.

The fixture projects paragraphs into fragments at a fixed UTF-16 offset. It has
no paginator: these checks isolate the editor/adapter contract from page layout.
They exercise native input, directional selection, structural edits, formatting,
undo/redo, and Chromium's native composition path, including paragraph edges and
empty paragraphs. The original run is always authoritative; the test verifies
full text and exported operations.
