# `viewport.js` checks

What the emitted zoom script does can only be seen in a browser, so these are
run by hand rather than by `odr_test`.

```bash
test/browser/viewport/serve      # extracts the script, serves on :8731
open http://localhost:8731/tests.html
```

`serve` lifts `viewport_js` out of `src/odr/internal/html/frontend.cpp`, so what
runs is what ships. `page.html` stands in for a rendered view: it writes the
`:root{--odr-fit;--odr-zoom}` and `body{zoom}` that `write_zoom_style` would.

Why the harness is shaped this way:

- **`?webkit=1`** divides an applied zoom back out of chromium's rects, which is
  what webkit returns — so one browser covers the rect space. It does not cover
  the scroll space: `restore()` reads deltas in viewport coordinates and hands
  them to `window.scrollBy`, which is only right if webkit's `scrollBy`/`scrollY`
  are in that same zoomed space. `rectFactor()` probes for the rect convention at
  runtime; nothing probes the scroll one, and here it is chromium's. So the pinch
  check is worth one run in real safari.
- **A pinch focus, not the top of the viewport.** `restore()` is re-asserted for
  30 frames, and that loop converges at `y = 1` whatever coordinate space it
  computes in; a focus 400px down does not.
- **`overflow-anchor: none`**, or chromium's own scroll anchoring covers for the
  script. Webkit has none.
- **Positions are read as `(scrollY + y) / zoom`**, never through the script's
  helpers, so a wrong answer cannot agree with itself.

Keep the tab on screen: the browser throttles `resize` and
`requestAnimationFrame` in a window that is not.
