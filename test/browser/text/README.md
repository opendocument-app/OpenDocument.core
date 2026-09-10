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
- **The gate and the collection are driven apart.** `beforeinput` is what
  refuses; `input` is what collects, because a script rewriting the page raises
  none — which is how a search highlighting nine matches leaves the log alone.
  No script can raise the pair the way a key does, so a check drives one or the
  other.

**Scripted editing is not the editing a reader does, which is why no check uses
`execCommand`.** Chrome's scripted path differs from its trusted-input path
twice over: it raises no cancelable `beforeinput`, so
`execCommand("insertParagraph")` splits a paragraph the gate would have
refused; and it dissolves a run whose whole text it replaces, leaving the new
text outside every address. Trusted input does neither — a real Enter is
refused, and typing over a whole run keeps the run, its address and its style.
Both were checked by hand in a browser, and neither is reachable by a reader.
