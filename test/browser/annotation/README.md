# `pdf-annotation.js` checks

What the emitted annotation script does can only be seen in a browser, so these
are run by hand rather than by `odr_test`.

```bash
test/browser/annotation/serve    # extracts script and style, serves on :8733
open http://localhost:8733/tests.html
```

`serve` lifts `pdf_annotation_js` and `pdf_annotation_css` out of
`src/odr/internal/html/frontend.cpp`, so what runs is what ships — renaming
either declaration breaks the harness.

`tests.html` stands in for a rendered pdf view: two `.p` pages laid out in
inches, each carrying the `data-odr-page` and `data-odr-space` the renderer
emits, with one selectable `.sr` run on each.

Why the harness is shaped this way:

- **The pages declare `[1 0 0 -1 0 792]`**, the map a us-letter page with no
  crop-box offset and no rotation produces. A quad's expected user-space `y` is
  then `792 - top`, which the checks compute by hand rather than through the
  script, so a wrong answer cannot agree with itself.
- **The quad's expected position is measured off the run's own client rect**,
  not off its css: a text range's rect follows the font's metrics, not the line
  box, so `top: 92pt` does not put the glyphs at 92pt.
- **Ink is driven with real `PointerEvent`s**, not by calling into the model, so
  the pointer path and the coordinate mapping are both covered.
- **`setPointerCapture` is stubbed out**: a synthetic event has no real pointer
  to capture and chromium throws on it.
- The blend check reads which of the two overlays a shape lands in
  (`svg.an-m` multiplies, `svg.an` does not) rather than sampling pixels.
