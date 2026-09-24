# text editing checks

What the embedded text editor does and refuses is visible only in a browser,
so these checks run by hand and not in `odr_test`.

```bash
test/browser/text/serve          # serves on :8734
open http://localhost:8734/tests.html
```

`serve` serves `document.css`, `editing.js`, `document.js` and `search.js` out
of `src/odr/internal/html/frontend/`, and `checks.js` out of `test/browser/`,
so the checks run the file the library embeds. `editing.js` goes first, as the
library writes it. `error-codes.js` is built from `src/odr/error_code.{hpp,cpp}`
by `test/browser/serve.py`.

`tests.html` holds the shapes the editor handles: two runs beside each other,
a run under a link, a run under a style-only wrapper, a paragraph with a
picture and no run, and the `<wbr>` / `<br>` line box the renderer ends every
paragraph with. The checks drive `beforeinput`, which a browser fires before
it changes anything. The editor cancels the event and splices the page itself,
so the markup stays what the renderer wrote and every change has an operation.

Rules the checks follow:

- `defaultPrevented` does not say whether an edit was taken, because the
  editor cancels the event on a refusal too. `input()` answers `"taken"` or
  `"refused"` from the refusal channel.
- `reset()` rebuilds the page between groups, clears the log and turns the
  mode on again, because every edit is a real edit.
- A synthetic `InputEvent` has an empty `getTargetRanges()`, so the editor
  falls back to the selection. An Android WebView gives it the same. Each
  check sets the selection first. `extendForDelete` covers a Backspace whose
  range the browser did not state: one character, or the paragraph boundary
  at the caret.
- Word and line deletes are not extended. Where a browser states no range for
  `deleteWordBackward`, nothing happens.
- Two identical refusals within two seconds are one event (`editing.js`), and
  a refusal is keyed by its run.
- Each group asserts the operations as well as the text: which ops, in which
  order, naming which ids. `test/src/document_edit_test.cpp` replays the same
  shapes in C++.
- Scope `paragraph` runs on the same fixture. The group sets
  `data-odr-editing-scope` on `<body>`, which the editor reads per edit.
- Formatting is driven two ways: `odr.editing.format` and `toggle` for a
  host's button, and a `formatBold` input for the chord. The checks read the
  `style` attribute back. `onSelectionChange` is checked by dispatching
  `selectionchange` by hand, because the browser raises it after the script.
- A composition is driven as a browser fires one: a `beforeinput` that cannot
  be cancelled, the run written by hand, and the `input` after it.
- No check uses `execCommand`. Chrome's scripted path raises no cancelable
  `beforeinput`, so `execCommand("insertParagraph")` bypasses the editor. A
  real Enter goes through the gate.
