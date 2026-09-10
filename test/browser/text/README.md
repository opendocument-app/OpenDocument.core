# text editing checks

What the emitted text editor allows and what it refuses can only be seen in a
browser, so these are run by hand rather than by `odr_test`.

```bash
test/browser/text/serve          # serves on :8734
open http://localhost:8734/tests.html
```

`serve` serves `document.css`, `editing.js` and `document.js` straight out of
`src/odr/internal/html/frontend/`, and `checks.js` out of `test/browser/`, so
what runs is the file the library embeds. `editing.js` goes first, as the
library writes it.

- **`tests.html`** — the mode makes the **whole view** editable, and the editor
  refuses what it cannot replay. The fixture holds the shapes that decision
  turns on: two runs beside each other, a run under a link, a run under a
  style-only wrapper, and a paragraph holding a picture and no run at all. The
  checks drive `beforeinput`, which is what a browser fires before it changes
  anything, so a prevented one is an edit that never happened.

Why the checks look the way they do:

- **A synthetic `InputEvent` carries no target range.** `getTargetRanges()` is
  empty on an event the page constructs, so the editor falls back to the
  selection — which is also what a browser lacking `getTargetRanges` gives it.
  That is why each check sets the selection first.
- **The repeat suppression is part of the contract.** Two identical refusals
  within two seconds are one event ([`editing.md`](../../docs/design/editing.md)
  decision 9), and a refusal is keyed by its run — so the two `newLine` checks
  sit in different runs, and the one outside every run asserts the prevented
  edit rather than a second event.
- **The collection is a `MutationObserver`**, which reports on a microtask, so
  the last checks wait a turn. `checks.js` retallies on every check for exactly
  this reason.

**A scripted `document.execCommand` can still get past the guard.** Chrome does
not fire a cancelable `beforeinput` for every command, so
`execCommand("insertParagraph")` splits a paragraph the editor would have
refused — leaving two elements under one `data-odr-path`. Trusted input does
not: a real Enter is refused, which is what a reader can reach. Verified by
hand on 2026-09-10.
