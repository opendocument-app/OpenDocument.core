# sheet checks

What the emitted sheet script does with a cell too narrow for its text can only
be seen in a browser, so these are run by hand rather than by `odr_test`.

```bash
test/browser/sheet/serve         # extracts the css and the script, serves on :8732
open http://localhost:8732/tests.html
```

`serve` lifts `document_css`, `spreadsheet_css` and `spreadsheet_js` out of
`src/odr/internal/html/frontend.cpp`, so what runs is what ships. The markup is
what `translate_sheet` writes, cut down to the shapes the script has to tell
apart: a cell that spills over an empty neighbour, one that is cut at its edge,
one that keeps the block a stated row height needs, and one that writes its
string straight into the `td`.

The point of the checks is that raising a cell shows all of it **without moving
anything**: the box goes out of flow, so no row changes height, and a click
inside it is for the text rather than for the cell.
