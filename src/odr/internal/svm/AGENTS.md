# AGENTS.md — `internal/svm`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file covers what
svm does differently, and why. [`README.md`](README.md) has the feature matrix
and the references.

## What it is

A StarView Metafile is the vector format that StarOffice, OpenOffice and
LibreOffice write object replacement images in: the picture of a chart or an
OLE object inside an odf or ooxml package. An `.svm` rarely arrives on its own.

The file is a signature (`VCLMTF`), a header, and a flat list of actions that
replay against a graphics state, in the shape of a Windows metafile. Two
layers:

- `svm_format.*` reads the binary format, one `read_*` per object and per
  action. Every record starts with a `VersionCompat` (version and length). The
  length is what lets the translator skip an action it does not implement.
- `svm_to_svg.*` replays the actions against a `Context` (the graphics state)
  and writes svg through `svg::SvgWriter`.

`SvmFile` is an `abstract::ImageFile` with `is_decodable()` false.
`html/image_file.cpp` translates it to svg and embeds that as a data url.

## There is no spec

The references, best first:

- LibreOffice [`SvmReader.cxx`](https://github.com/LibreOffice/core/blob/master/vcl/source/filter/svm/SvmReader.cxx):
  the binary layout, per action, per version. The authority when a field is
  in doubt.
- LibreOffice [`svgwriter.cxx`](https://github.com/LibreOffice/core/blob/master/filter/source/svg/svgwriter.cxx):
  its own metafile to svg export. The reference for mapping decisions.
- [`metaact.hxx`](https://github.com/LibreOffice/core/blob/master/include/vcl/metaact.hxx):
  what each action means.
- ONLYOFFICE's [`SPEC`](https://github.com/ONLYOFFICE/core/blob/master/DesktopEditor/raster/Metafile/StarView/SPEC):
  a prose write-up modelled on [MS-WMF]. Incomplete: `Color` and the polygon
  flags are unfinished.

## Conventions

- An unimplemented action is skipped by its length, never guessed at. If the
  reader stops short, the rest of the action is ignored and logged. If it
  reads past the end, the file is malformed and we throw.
- A `POP` restores only what its `PUSH` named. Half the pushes in the corpus
  save a subset of `PushFlags`.
- Everything unhandled is logged. A metafile we cannot draw looks exactly like
  one we drew correctly, so the log is the only way to tell. Every `default:`
  says so.
- The markup goes through `svg::SvgWriter`, never to the stream directly.
  Text in a label is arbitrary, and one unescaped `&` costs the whole image.
  `html::escape_text` is the wrong escape, because it emits `&nbsp;`.
- A byte string is decoded by the charset the last `FONT` action named.
  Passing the bytes through emits invalid utf-8 for anything but ascii, which
  an xml parser refuses. An encoding we cannot decode is taken for `MS_1252`.

## What the corpus holds

Over 1125 metafiles harvested from the `odt` and `ods` fixtures. Every
"occurs nowhere" in this module is measured against them.

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

Two thirds of every action is text. `ELLIPSE`, `ARC`, `PIE`, `CHORD`,
`ROUNDRECT`, `POINT`, `PIXEL`, `GRADIENT`, `HATCH`, `TRANSPARENT` and `EPS` do
not occur, nor does a polygon flag or a complex poly-polygon. The one bitmap is
the data area of `odr-private/svm/Vyplaty.svm`. So a change here is proved by
`svm_test.cpp` and by LibreOffice, not by the reference output.

## Bitmaps do not go through a decoder

A dib in a metafile carries its own `BITMAPFILEHEADER`, so it is a `.bmp`
file. Only its length is needed, which the header's `biSizeImage` gives and
`bfSize` does not, because `bfSize` is written from the uncompressed size.
Where the pixels are plain enough to copy out row by row, they are re-packed
as a png, which is far smaller for a chart. A compressed dib goes out as the
bmp it is.

A `BMPEX` may carry a transparency mask: a second dib, white where the bitmap
does not show. An svg `<mask>` keeps what is white, so the mask image goes
through an inverting `feColorMatrix` (`filter="url(#odr-invert)"`), written
once per document.

`FLOATTRANSPARENT` nests a whole metafile. It is not implemented. The shape
is: translate the nested metafile into a `<g>` and put the gradient on that
group's `mask`.

## Testing

`svm_test.cpp` builds its input as bytes inline through `SvmBuilder`, so an
action is testable without a fixture. The fixtures
`odr-public/svm/{chart-1,table-1}.svm`, `odr-private/svm/{test,Vyplaty}.svm`
and the `odt` and `ods` files named `*svm*` are the end-to-end check.

LibreOffice renders the same file, so it is the oracle for the drawing:

```sh
/Applications/LibreOffice.app/Contents/MacOS/soffice --headless \
  --convert-to svg --outdir /tmp test/data/input/odr-public/svm/chart-1.svm
```

Its player is the oracle, not that export. The export drops an action and
misplaces another on a metafile that switches to twips and then to a relative
map mode. Where the two disagree, follow the vcl source.
