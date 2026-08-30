# PLAN — `internal/svm`

What is missing from the svm → svg translator and the order it gets fixed in.
Umbrella issue: [#772](https://github.com/opendocument-app/OpenDocument.core/issues/772),
sub-parts [#194](https://github.com/opendocument-app/OpenDocument.core/issues/194)
(bitmaps) and [#95](https://github.com/opendocument-app/OpenDocument.core/issues/95)
(font attributes).

Read [`AGENTS.md`](AGENTS.md) for how the module is built and what the
references are.

## Stages

Each stage is one pull request, stacked on the one before it.

1. **Infrastructure.** `svg::SvgWriter`, so markup is written by something that
   escapes; a `Logger` through the translator, so an action we drop says so;
   inline-bytes tests, so an action can be tested without a fixture. Fixes
   #772's defects 1 (escaping), 9 (style dispatch) and 10 (silence).
2. **Fixes to what we already emit.** The graphics state stack (`PUSH`/`POP`),
   poly-polygon fill rule, the font size and map-mode unit in the transform,
   `LineInfo`. #772 defects 2, 3, 5, 6, 8.
3. **Text.** `TEXTALIGN`, the `TEXTARRAY` dx array, `TEXTRECT`, and the #95
   font attributes (bold, italic, underline, strikeout, family).
4. **Primitives.** `PIXEL`, `POINT`, `LINE`, `ROUNDRECT`, `ELLIPSE`, `ARC`,
   `PIE`, `CHORD` — one `svgwriter.cxx` case each.
5. **Bitmaps** (#194). See the shortcut below.
6. **Fills, clipping, transparency.** `GRADIENT`, `GRADIENTEX`, `HATCH`,
   `WALLPAPER`, the `CLIPREGION` family, `TRANSPARENT`, `FLOATTRANSPARENT`.
7. **Stretch.** Bézier flags (#772 defect 4), the `EPS` substitute metafile,
   and version-1 (pre-`VCLMTF`) files via `SvmConverter.cxx`.

## Shortcuts worth taking

- **Bitmaps are `.bmp` files already.** `SvmReader` reads them with
  `ReadDIB(…, bFileHeader=true)`, i.e. the action body holds a DIB *with* its
  `BITMAPFILEHEADER` — `"BM"`, `bfSize`, `bfOffBits`. So a `BMP` action needs
  no pixel decoding at all: read the header far enough to know the byte length,
  hand the bytes to the browser as `data:image/bmp;base64,…` inside an
  `<image>`. Palettes, RLE4/RLE8 and bit fields are then the browser's problem,
  not ours. Two cases still need work: `ZCOMPRESS` (a LibreOffice-only
  compression — inflate with miniz, then rewrite the header), and the alpha
  mask of `BMPEX` (a second DIB, `1` = transparent) which becomes an SVG
  `<mask>` over an inverting `feColorMatrix`.
- **Béziers are cheap once the flags are read.** A polygon flag of
  `PolyFlags::Control` marks a control point, so a flagged polygon maps onto an
  SVG path's `C` segments directly. The reader is the part that is missing.
- **Gradients, hatches and dashes are declarative in SVG** —
  `<linearGradient>`, `<radialGradient>`, `<pattern>`, `stroke-dasharray`. No
  rasterising, no tiling by hand.
- **`FLOATTRANSPARENT` nests a whole metafile**: translate it into a `<g>` and
  put the gradient on that group's `mask`.

## Testing

`svm_test.cpp` builds its input as bytes inline (`SvmBuilder`), so an action
gets a test without a fixture. The fixtures
(`odr-public/svm/{chart-1,table-1}.svm`, `odr-private/svm/{test,Vyplaty}.svm`)
stay the end-to-end check, and LibreOffice is the oracle for what the drawing
should look like:

```sh
/Applications/LibreOffice.app/Contents/MacOS/soffice --headless \
  --convert-to svg --outdir /tmp test/data/input/odr-public/svm/chart-1.svm
```
