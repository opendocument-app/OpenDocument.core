# text editing checks

What the emitted text editor does and what it refuses can only be seen in a
browser, so these are run by hand rather than by `odr_test`.

```bash
test/browser/text/serve          # serves on :8734
open http://localhost:8734/tests.html
```

`serve` serves `document.css`, `editing.js` and `document.js` straight out of
`src/odr/internal/html/frontend/`, and `checks.js` out of `test/browser/`, so
what runs is the file the library embeds. `editing.js` goes first, as the
library writes it.

- **`tests.html`** — the editor **owns the edit**: it cancels what the browser
  was about to do and splices the page itself, so the markup stays what the
  renderer wrote and every change has an operation behind it. The fixture holds
  the shapes that turns on: two runs beside each other, a run under a link, a
  run under a style-only wrapper, a paragraph holding a picture and no run at
  all, and the `<wbr>` / `<br>` line box the renderer ends every paragraph
  with. The checks drive `beforeinput`, which is what a browser fires before it
  changes anything.

Why the checks look the way they do:

- **`defaultPrevented` no longer says whether an edit was taken.** The editor
  cancels the event either way — once because it is doing the edit itself, once
  because it is refusing. So `input()` answers `"taken"` or `"refused"` by
  watching the refusal channel, and the cancelling is checked once on its own.
- **The page is rebuilt between groups.** Every edit is a real edit, so a group
  that ran before would decide what the next one starts from. `reset()` puts
  the fixture back, clears the log and turns the mode on again.
- **A synthetic `InputEvent` carries no target range.** `getTargetRanges()` is
  empty on an event the page constructs, so the editor falls back to the
  selection — which is also what a browser lacking `getTargetRanges` gives it,
  and what an Android WebView is reported to give it. That is why each check
  sets the selection first, and it is the path `extendForDelete` exists for: a
  Backspace whose range the browser did not state is one character, or the
  paragraph boundary the caret stands at.
- **The word and line deletes are not extended.** Where a browser states no
  range for `deleteWordBackward`, guessing where the word ends would take away
  text the reader did not name, so nothing happens.
- **The repeat suppression is part of the contract.** Two identical refusals
  within two seconds are one event ([`editing.md`](../../docs/design/editing.md)
  decision 9), and a refusal is keyed by its run.
- **The log is checked, not only the page.** What a save hands to
  `Document::edit` is the point of the editor, so each group asserts the
  operations as well as the text: which ops, in which order, naming which ids.
  `document_edit_test.cpp` replays the same shapes in C++, which is what keeps
  the two sides from drifting apart.

**Scripted editing is not the editing a reader does, which is why no check uses
`execCommand`.** Chrome's scripted path raises no cancelable `beforeinput`, so
`execCommand("insertParagraph")` splits a paragraph without the editor ever
seeing it. Trusted input does not; a real Enter goes through the gate. Checked
by hand in a browser, and not reachable by a reader.
