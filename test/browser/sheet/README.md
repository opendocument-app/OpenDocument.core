# sheet checks

What the emitted sheet scripts do with a cell too narrow for its text, and with
a position no `td` stands at, can only be seen in a browser, so these are run by
hand rather than by `odr_test`.

```bash
test/browser/sheet/serve         # extracts the css and the scripts, serves on :8732
open http://localhost:8732/tests.html
open http://localhost:8732/positions.html
open http://localhost:8732/sorting.html
```

`serve` lifts `document_css`, `spreadsheet_css`, `spreadsheet_js` and
`sheet_editing_js` out of `src/odr/internal/html/frontend.cpp`, so what runs is
what ships. Each page prints its own report and heads it with a count; a page
holds one `.odr-sheet`, because the script binds to the first one it finds.

- **`tests.html`** — raising a cell whose text is cut off. The markup is what
  `translate_sheet` writes, cut down to the shapes the script has to tell apart:
  a cell that spills over an empty neighbour, one that is cut at its edge, one
  that keeps the block a stated row height needs, and one that writes its string
  straight into the `td`. The point is that raising shows all of a cell
  **without moving anything**: the box goes out of flow, so no row changes
  height, and a click inside it is for the text rather than for the cell.
- **`positions.html`** — `odr.sheet` over a merged sheet. `translate_sheet`
  writes no `td` for a position a span covers, so the fixture has a `colspan`, a
  `rowspan`, and a `rowspan` reaching past the last cell of the row below it:
  the three shapes a walk over `colspan` alone reads wrong. It also checks that
  `odr.editing` finds a lock through the same map.
- **`sorting.html`** — the same questions after the sort control has moved every
  row. Nothing here is merged, because a merged sheet is offered no sort
  control; a row is found by the label it carries, so where it now sits does not
  matter.
