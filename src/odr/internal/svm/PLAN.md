# PLAN — `internal/svm`

What is missing from the svm → svg translator and the order it gets fixed in.
Umbrella issue: [#772](https://github.com/opendocument-app/OpenDocument.core/issues/772),
sub-parts [#194](https://github.com/opendocument-app/OpenDocument.core/issues/194)
(bitmaps) and [#95](https://github.com/opendocument-app/OpenDocument.core/issues/95)
(font attributes).

Read [`AGENTS.md`](AGENTS.md) for how the module is built and
[`README.md`](README.md) for the feature matrix and the references.

## Stages

Each stage is one pull request, stacked on the one before it.

1. **Infrastructure.** `svg::SvgWriter`, so markup is written by something that
   escapes; a `Logger` through the translator, so an action we drop says so;
   inline-bytes tests, so an action can be tested without a fixture. Fixes
   #772's defects 1 (escaping, but see stage 3 for the encoding half of it), 9
   (style dispatch) and 10 (silence).
2. **Fixes to what we already emit.** The graphics state stack (`PUSH`/`POP`),
   the fill that killed the stroke, the poly-polygon fill rule, the font size
   in the transform, `LineInfo`. #772 defects 2, 3, 5, 8.
3. **Text.** `TEXTALIGN`, the `TEXTARRAY` dx array, the `STRETCHTEXT` width,
   the run a text action names, and the #95 font attributes (bold, italic,
   underline, strikeout, rotation). `TEXTRECT` is still open - it occurs
   nowhere in the corpus - and so is decoding a non-`UCS2` string instead of
   passing its bytes through: until then a latin-1 label emits invalid utf-8,
   which costs the image exactly as an unescaped `&` did.
4. **Clipping.** `CLIPREGION`, `ISECTRECTCLIPREGION`,
   `ISECTREGIONCLIPREGION`, `MOVECLIPREGION`.
5. **Bitmaps** (#194). See the shortcut below.
6. **Primitives.** `PIXEL`, `POINT`, `LINE`, `ROUNDRECT`, `ELLIPSE`, `ARC`,
   `PIE`, `CHORD` — one `svgwriter.cxx` case each.
7. **Fills and transparency.** `GRADIENT`, `GRADIENTEX`, `HATCH`,
   `WALLPAPER`, `TRANSPARENT`, `FLOATTRANSPARENT`.
8. **The map mode's unit** (#772 defect 6), see below.
9. **Stretch.** Bézier flags (#772 defect 4), the `EPS` substitute metafile,
   and version-1 (pre-`VCLMTF`) files via `SvmConverter.cxx`.

The order follows what files actually contain, not the action list. Over 1125
metafiles harvested from the `odt`/`ods` fixtures:

| action | occurrences | files (of 1125) |
| --- | --- | --- |
| `PUSH` / `POP` | 22317 each | 1124 |
| `TEXTALIGN` | 21534 | 1117 |
| `STRETCHTEXT` | 20335 | 1097 |
| `ISECTRECTCLIPREGION` | 1124 | 1123 |
| `RECT` | 845 | 324 |
| `TEXTARRAY` | 764 | 20 |
| `POLYLINE` | 477 | 13 |
| `POLYPOLYGON` | 247 | 14 |
| `LINE` | 47 | 2 |
| `BMPEXSCALE` | 1 | 1 |

Half the text in the corpus is italic, all of it a formula variable, which is
what puts the #95 font attributes in stage 3 rather than later.

`ELLIPSE`, `ARC`, `PIE`, `CHORD`, `ROUNDRECT`, `POINT`, `PIXEL`, `GRADIENT`,
`HATCH`, `TRANSPARENT` and `EPS` do not occur at all, which is why they come
after clipping rather than before it. The one bitmap is the whole data area of
`odr-private/svm/Vyplaty.svm` — 1.59 MB of `BMPEXSCALE` in a 1.63 MB file, and
the reason that chart renders as an empty frame today.

## The map mode

Deferred, and not just an oversight — the units are one part of a bigger
question. `MetaMapModeAction::Execute` calls `OutputDevice::SetMapMode`, which
*replaces* the map mode, **except** where the new one's unit is
`MapUnit::MapRelative` (13): then its scales multiply the current ones and its
origin offsets the current one. We replace unconditionally, and we ignore the
unit, so a `MAPMODE` action that switches from 100th mm to twips is off by a
factor of 1.76.

It is rare — 4 `MAPMODE` actions in 1125 files, one of them relative — and
getting it right means following `vcl/source/outdev/map.cxx` rather than
guessing, so it is its own stage.

## Text is where the corpus lives

Two thirds of every action in the corpus is text, and three things about it
are worth writing down:

- **`TextAlign` is vertical only.** It says whether the draw point is the top,
  the baseline or the bottom of the run; vcl has no horizontal text alignment,
  a run always starts at the point. Its default is `ALIGN_TOP`, which is not
  what an svg `<text>` does, so it has to be written out. `svgwriter.cxx`
  shifts the point by the font's ascent because it has the metrics; we name
  `dominant-baseline` and let the browser do it.
- **A text action names a run**, `(index, length)`, of the string it carries -
  and the string is the whole paragraph. Drawing the string rather than the run
  overprints the sentence at every run's position.
- **The dx array and the stretch width are the file's own measurements**, and
  they are what keeps a formula together when the viewer's font is not the
  author's. They map onto an `x` list and `textLength` respectively.

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
