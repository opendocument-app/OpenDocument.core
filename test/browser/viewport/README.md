# `viewport.js` checks

What the embedded zoom script does is visible only in a browser, so these
checks run by hand and not in `odr_test`.

```bash
test/browser/viewport/serve      # serves on :8731
open http://localhost:8731/tests.html
```

`serve` serves `viewport.js` out of `src/odr/internal/html/frontend/`, so the
checks run the file the library embeds. `page.html` stands in for a rendered
view. It writes the `:root{--odr-fit;--odr-zoom}` and `body{zoom}` that
`write_zoom_style` writes.

Rules the checks follow:

- `?webkit=1` divides an applied zoom back out of chromium's rects, which is
  what webkit returns. So one browser covers the rect space. It does not cover
  the scroll space: `restore()` hands deltas in viewport coordinates to
  `window.scrollBy`, and `rectFactor()` probes only the rect convention. The
  pinch check is worth one run in real safari.
- The pinch focus is 400px down, not at the top. `restore()` runs for 30
  frames, and that loop converges at `y = 1` in any coordinate space.
- `page.html` sets `overflow-anchor: none`, because chromium's own scroll
  anchoring hides script errors. Webkit has none.
- Positions are read as `(scrollY + y) / zoom`, never through the script's
  helpers.

Who fits the width is the one thing a frame cannot check, because `--odr-fit`
`auto` and `view` both measure in one. `tests.html` links the two top-level
pages that can, and each prints its own verdict.

Keep the tab on screen. The browser throttles `requestAnimationFrame` in a
window that is not. The harness dispatches the scroll and resize events
itself, but not the settling frames after them.

Two webkit rules stay uncovered, both about type under an applied `zoom`
(#761): webkit holds the text at its unscaled size unless the zoom is stated
back as `text-size-adjust`, by a factor its cluster heuristics decide, and it
draws no text below 9px. The oracle is a document served to a real webkit.
