# AGENTS.md — `internal/svm`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file covers what
svm does differently, and why. [`README.md`](README.md) has the feature matrix
and the references.

## What it is

A StarView Metafile is the vector format StarOffice, OpenOffice and
LibreOffice write **object replacement images** in: the picture of a chart or
an OLE object that an odf or ooxml package carries alongside the object itself.
So an `.svm` rarely arrives on its own — it arrives inside a document, and it
is what the reader sees where a chart should be.

The file is a signature (`VCLMTF`), a header, and then a flat list of *actions*
replayed against a graphics state, in the shape of a Windows metafile. Two
layers here:

- `svm_format.*` — the binary reader: one `read_*` per object and per action.
  Every record starts with a `VersionCompat` (version + length), and the
  length is what lets the translator skip an action it does not implement.
- `svm_to_svg.*` — replays the actions against a `Context` (the graphics
  state) and writes svg through `svg::SvgWriter`.

`SvmFile` is an `abstract::ImageFile` and `is_decodable()` is false: there is
no element tree, and `html/image_file.cpp` renders it by translating it to svg
and embedding that as a data url.

## There is no spec

The format is not specified anywhere. The references, best first:

- LibreOffice
  [`SvmReader.cxx`](https://github.com/LibreOffice/core/blob/master/vcl/source/filter/svm/SvmReader.cxx)
  — the authoritative binary layout, per action, per version. When a field's
  meaning is in doubt, this is the answer.
- LibreOffice
  [`svgwriter.cxx`](https://github.com/LibreOffice/core/blob/master/filter/source/svg/svgwriter.cxx)
  — its own metafile → svg export, i.e. our problem already solved, for 52
  action types. The reference for *mapping* decisions.
- [`metaact.hxx`](https://github.com/LibreOffice/core/blob/master/include/vcl/metaact.hxx)
  — what each action means.
- ONLYOFFICE's
  [`SPEC`](https://github.com/ONLYOFFICE/core/blob/master/DesktopEditor/raster/Metafile/StarView/SPEC)
  — a prose write-up modelled on [MS-WMF], reverse-engineered from the same
  sources. Cheap to read, but incomplete: several FIXMEs, `Color` unfinished,
  polygon flags explicitly unfinished.

## Conventions

- **An unimplemented action is skipped by its length, never guessed at.** The
  loop checks how far the reader got: short means the rest is ignored (logged),
  past the end means the file is malformed and we throw.
- **A `POP` restores only what its `PUSH` named.** Half the pushes in the
  corpus save a subset of `PushFlags`, so restoring the whole state would be
  wrong more often than right.
- **Everything unhandled is logged.** A metafile we cannot draw looks exactly
  like one we drew correctly — a blank rectangle raises no error anywhere — so
  the log is the only way to tell. Anything reached by `default:` says so.
- **The markup goes through `svg::SvgWriter`, never to the stream directly.**
  Text in a chart label is arbitrary, so it is escaped, and svg is xml: an
  unescaped `&` costs the whole image, not one label. Note that
  `html::escape_text` is the *wrong* escape here — it emits `&nbsp;`, which no
  xml parser knows.
- **Decoding is the other half of escaping.** A byte string is decoded by the
  charset the last `FONT` action named, because handing the bytes through
  emits invalid utf-8 for anything but ascii and an xml parser refuses that
  exactly as hard as an unescaped `&`. An encoding we have no decoder for is
  taken for `MS_1252` rather than passed through.

## What the corpus holds

Over 1125 metafiles harvested from the `odt`/`ods` fixtures, which is what
every "occurs nowhere" in this module is measured against:

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

Two thirds of every action is text, and half of that text is italic — all of
it a formula variable. `ELLIPSE`, `ARC`, `PIE`, `CHORD`, `ROUNDRECT`, `POINT`,
`PIXEL`, `GRADIENT`, `HATCH`, `TRANSPARENT` and `EPS` do not occur at all, nor
does a polygon flag or a complex poly-polygon. The one real bitmap is the
whole data area of `odr-private/svm/Vyplaty.svm` — 1.59 MB of `BMPEXSCALE` in
a 1.63 MB file.

So a change here is proved by `svm_test.cpp` and by LibreOffice, not by the
reference output: most of what the module now draws, no fixture exercises.

## `FLOATTRANSPARENT` nests a whole metafile

The one unimplemented action whose shape is already worked out: translate the
nested metafile into a `<g>` and put the gradient on that group's `mask`.

## Bitmaps do not go through a decoder

A dib in a metafile carries its own `BITMAPFILEHEADER`, so it *is* a `.bmp`
file — no pixel decoding is needed to show one, only its length, which the
header's `biSizeImage` gives and whose `bfSize` does not (that one is written
from the uncompressed size). Where the pixels are plain enough to copy out row
by row they are re-packed as a png, which for a chart is fifty times smaller;
anything compressed goes out as the bmp it is.

A `BMPEX` may carry a transparency mask: a second dib, white where the bitmap
does *not* show. An svg `<mask>` keeps what is white, so the mask image goes
through an inverting `feColorMatrix` — `filter="url(#odr-invert)"`, written
once per document.

## Testing

`svm_test.cpp` builds its input as bytes inline through `SvmBuilder`, so an
action is testable without a fixture and the test says what the bytes mean.
The fixtures — `odr-public/svm/{chart-1,table-1}.svm`,
`odr-private/svm/{test,Vyplaty}.svm`, and the `odt`/`ods` files named `*svm*`
— stay the end-to-end check.

LibreOffice renders the same file, which makes it the oracle for what the
drawing should look like:

```sh
/Applications/LibreOffice.app/Contents/MacOS/soffice --headless \
  --convert-to svg --outdir /tmp test/data/input/odr-public/svm/chart-1.svm
```

Its *player* is the oracle, not that export: asked to convert a metafile that
switches to twips and then to a relative map mode, the export drops an action
and places another outside the box. Where the two disagree, follow the vcl
source.
