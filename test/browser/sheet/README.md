# sheet checks

What the embedded sheet scripts do with a cell too narrow for its text, and
with a position no `td` stands at, is visible only in a browser, so these
checks run by hand and not in `odr_test`.

```bash
test/browser/sheet/serve         # serves on :8732
open http://localhost:8732/tests.html
open http://localhost:8732/positions.html
open http://localhost:8732/sorting.html
open http://localhost:8732/editing.html
open http://localhost:8732/keyboard.html
```

`serve` serves `document.css`, `spreadsheet.css`, `editing.js`,
`spreadsheet.js` and `sheet-editing.js` out of
`src/odr/internal/html/frontend/`, so the checks run the file the library
embeds. `error-codes.js` is built from `src/odr/error_code.{hpp,cpp}` by
`test/browser/serve.py`. Each page prints its own report with a count. A page
holds one `.odr-sheet`, because the script binds to the first one it finds.

`editing.js` goes first, as the library writes it. It owns `odr.editing` and
`odr.takesKeys`, and the other two scripts read them. The page states the
frame on `<body>` with `data-odr-editable` and `data-odr-keyboard`, where
`translate` writes it.

- `tests.html`: raising a cell whose text is cut off. The markup is what
  `translate_sheet` writes, cut down to the shapes the script must tell apart:
  a cell that spills over an empty neighbour, one cut at its edge, one that
  keeps the block a stated row height needs, and one that writes its string
  into the `td`. Raising shows all of a cell without moving anything: the box
  goes out of flow, no row changes height, and a click inside it is for the
  text.
- `positions.html`: `odr.sheet` over a merged sheet. `translate_sheet` writes
  no `td` for a position a span covers, so the fixture has a `colspan`, a
  `rowspan`, and a `rowspan` that reaches past the last cell of the row below.
  It also checks that `odr.editing` finds a lock through the same map.
- `editing.html`: the overlay editor, driven through `odr.editing` as a host
  drives it. The shapes: a string cut where its neighbour shows something, a
  formula cell, a cell of several runs, and one whose single run carries a
  style a write must keep. Then undo, redo, the log a save resets, and the
  marks a commit leaves on the formula cells that read what it wrote.
- `keyboard.html`: a page whose config took both key classes away. The
  arrows, Escape, a printable key and the undo chord belong to the host. The
  commands (`editAt`, `undo`) and the keys of an open editor still work.
- `sorting.html`: the same questions after the sort control moved every row.
  Nothing is merged, because a merged sheet gets no sort control. A row is
  found by its label, not by its position.
