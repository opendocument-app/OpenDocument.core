# AGENTS.md — `internal/svm`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file covers what
svm does differently, and why. [`README.md`](README.md) has the feature matrix
and the references, [`PLAN.md`](PLAN.md) the roadmap.

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
- **Escaping is not yet enough.** `read_string_with_encoding` hands back the
  file's own bytes for every encoding but `UCS2`, so a latin-1 label emits
  invalid utf-8 and the parser refuses the document all the same.

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
