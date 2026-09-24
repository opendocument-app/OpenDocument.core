# plain-text editing checks

The editor of the `.txt` view is `text.js`, attached to `odr.editing`. See
[`txt-editing.md`](../../../docs/design/txt-editing.md). The checks run by hand,
like the rest of `test/browser/`.

```bash
test/browser/plaintext/serve     # serves on :8735
open http://localhost:8735/tests.html
```

`serve` serves `text.css`, `editing.js` and `text.js` out of
`src/odr/internal/html/frontend/`, so the checks run the file the library
embeds. `editing.js` goes first, as the library writes it. `error-codes.js` is
built from `src/odr/error_code.{hpp,cpp}` by `test/browser/serve.py`, because
the library writes that table into the page.

`tests.html` is three lines and the gutter beside them, without
`contenteditable`. The mode puts that on.

Rules the checks follow:

- The log is one `setContent` operation, however long the session runs. There
  are no ids, because there are no elements.
- The gutter is checked with the lines. The line numbers track the lines
  whether editing is on or off.
- Two identical refusals within two seconds are one event (`editing.js`). The
  second read-only check asserts that the edit did not land, not a second
  event.
- The envelope version is asserted. It is the one thing the browser and
  `Document::edit` must agree on.
