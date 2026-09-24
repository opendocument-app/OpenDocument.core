# `pdf-annotation.js` checks

What the embedded annotation script does is visible only in a browser, so
these checks run by hand and not in `odr_test`.

```bash
test/browser/annotation/serve    # serves on :8733
open http://localhost:8733/tests.html
```

`serve` serves `pdf-annotation.js` and `pdf-annotation.css` out of
`src/odr/internal/html/frontend/`, so the checks run the file the library
embeds. `checks.js` comes from `test/browser/`.

`tests.html` stands in for a rendered pdf view: two `.p` pages laid out in
inches, each with the `data-odr-page` and `data-odr-space` the renderer writes,
and one selectable `.sr` run on each.

Rules the checks follow:

- The pages declare `[1 0 0 -1 0 792]`, the map of a us-letter page with no
  crop-box offset and no rotation. The checks compute the expected user-space
  `y` as `792 - top` by hand, not through the script.
- The expected position of a quad comes from the run's own client rect, not
  from its css. A text range's rect follows the font metrics, not the line box.
- Ink is driven with real `PointerEvent`s, so the pointer path and the
  coordinate mapping are both covered.
- `setPointerCapture` is stubbed, because chromium throws on a synthetic event.
- The blend check reads which overlay a shape lands in (`svg.an-m` multiplies,
  `svg.an` does not). It does not sample pixels.
- A drag is stepped by hand: pointer down, the selection extended one
  character at a time, pointer up. Chromium does not select text from a
  synthetic mouse event, and one `addRange` fires one `selectionchange`. This
  is the one place that sets `markOnSelection`. Elsewhere the checks call
  `mark()`.
- Nothing waits on a frame, because a background window throttles
  `requestAnimationFrame`. The live stroke is asserted after `pointerup`.
