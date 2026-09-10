# plain-text editing checks

The `.txt` view's editor, which is `text.js` attached to `odr.editing`; see
[`txt-editing.md`](../../../docs/design/txt-editing.md). Run by hand, like the
rest of `test/browser/`.

```bash
test/browser/plaintext/serve     # serves on :8735
open http://localhost:8735/tests.html
```

`serve` serves `text.css`, `editing.js` and `text.js` straight out of
`src/odr/internal/html/frontend/`, so what runs is the file the library embeds.
`editing.js` goes first, as the library writes it.

- **`tests.html`** — a plain-text view carries the same mode every other view
  does, and `text.js` is its editor rather than a second API. The fixture is
  three lines and the gutter beside them, without `contenteditable`: the mode
  puts that on.

Why the checks look the way they do:

- **A plain file's whole content is its document**, so the log is one
  `setContent` operation however long the session — no ids, because there are
  no elements to name.
- **The gutter is checked alongside the lines.** `text.js` does double duty:
  the line numbers track the lines whether or not editing is on, and a split or
  a join that left them behind would be invisible in the text alone.
- **A refusal repeats.** Two identical refusals within two seconds are one
  event ([`editing.md`](../../docs/design/editing.md) decision 9), so the
  second read-only check asserts the edit did not land rather than a second
  event.
- **The envelope's version is asserted.** It is the one thing the browser and
  `Document::edit` have to agree on that neither side would notice drifting —
  and it *did* drift once.
