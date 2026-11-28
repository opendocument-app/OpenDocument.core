# Pagination checks

For the interactive example, generate its document with a built `pyodr` on
`PYTHONPATH` and the interpreter it was built for:

```bash
PYTHONPATH=cmake-build-relwithdebinfo/python python test/browser/pagination/make-demo.py
test/browser/pagination/serve
```

Open <http://127.0.0.1:8735/demo.html>. The document is rendered by core with
editing enabled and links the frontend scripts rather than embedding them, so
`serve` answers with the source files and a script change needs no new render.
Its generated HTML is ignored by git. Select **Show first
split**, **Edit document**, then type on either side of the split. Undo and Redo
operate on real editor history. Reset reloads the original example.

Run `test/browser/pagination/serve`, then open
<http://127.0.0.1:8735/tests.html>. Keep the tab visible. The harness loads the
actual frontend assets and reports PASS/FAIL in `#results`; automation can
wait for `body[data-test-result]` to become `pass` or `fail`.

A 2,000-paragraph source checks that work stops beyond the viewport. Shorter
sources cover manual breaks, different page dimensions, first/continuing
headers, footer placement, oversized content and runtime options. A long
styled paragraph covers measured line splitting, Unicode preservation, node
identity, selection and clipboard mapping, live editing and search transitions.
Print events check source restoration, not a browser's PDF generation.

This is a real-layout test; a DOM-only emulator cannot verify pagination.
See `docs/design/pagination.md` for the supported subset and remaining work.

The native input check uses Playwright and the generated demo. With the server
running, set `PLAYWRIGHT_MODULE` to an installed `playwright`/`playwright-core`
module if it is not available on Node's module path. `BROWSER_EXECUTABLE` selects
a local browser; `BROWSER=firefox` switches engines (the default is Chromium).

```bash
node test/browser/pagination/check-editing.cjs /tmp/paginated-edit.json
PYTHONPATH=cmake-build-relwithdebinfo/python python \
  test/browser/pagination/replay-editing.py /tmp/paginated-edit.json
```

The first command checks real typing, cursor movement, Backspace, Enter and
undo/redo; Chromium additionally exercises native IME updates and replacement
across a page edge. It exports operations and expected source text/formatting.
The second replays those operations into core, saves/reopens the generated
`demo-document.fodt`, and compares paragraphs and run formatting. Synthetic
composition cases also run in `tests.html`. Safari/WebKit and physical mobile
keyboards still need validation.

`check-scroll.cjs` uses the same Playwright environment variables. It types at
both sides of the first page split and with a margin at the top of the viewport,
checking scroll position and caret visibility after typing, Backspace, undo/redo
and deferred reflow. A forced one-block frame budget covers intermediate layouts:

```bash
node test/browser/pagination/check-scroll.cjs
```

`check-composition.cjs` uses Chromium's native IME path to check scope and
protected-field refusals, plus Undo, formatting, search, printing and disabling
editing during composition. It also checks the fallback for a missing
`compositionend` event and noncancelable replacement in a split run:

```bash
node test/browser/pagination/check-composition.cjs
```

`check-fit.cjs` checks initial and runtime fitting in a narrow iframe, later wider
pages, actual size, explicit width, and initial/manual zoom pins. It loads the
frontend sources directly and needs no server; it uses the same Playwright
variables and supports Chromium and Firefox:

```bash
node test/browser/pagination/check-fit.cjs
```
