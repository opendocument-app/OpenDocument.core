# SVM implementation

StarView Metafile → SVG. See [`AGENTS.md`](AGENTS.md) for how the module is
built, what it does differently, and what the corpus actually contains.

## Features

38 of the 54 action types we name are drawn.

- [x] shapes
  - [x] rectangle, rounded rectangle
  - [x] polyline, polygon, poly-polygon, with the fill rule that cuts its holes
  - [x] pixel, point, line, ellipse, arc, pie, chord
  - [x] bézier segments
- [x] colour
  - [x] line, fill, text
  - [x] `LineInfo` (width, dash, join)
  - [ ] text fill, overline (read into the state, never drawn)
- [x] font
  - [x] family, size, colour
  - [x] italic, bold, underline, strike through, rotation
  - [x] alignment — `TextAlign` is vertical only, a run always starts at the
        point
- [x] text
  - [x] `TEXT`, `TEXTARRAY`, `STRETCHTEXT`, and the run each of them names
  - [x] the `TEXTARRAY` dx array and the `STRETCHTEXT` width
  - [x] non-`UCS2` encodings, decoded to utf-8
  - [ ] `TEXTRECT`, `TEXTLINE` — the reader has both, nothing draws them
- [x] map mode
  - [x] origin, scale, unit
  - [x] the relative map mode, which composes with the one before it
- [x] images
  - [x] `BMP`, `BMPEX` and their scale and part variants, transparency mask
        included
  - [ ] the `MASK` family, which stencils one colour through a bitmap
  - [ ] `ZCOMPRESS`ed dibs, whose zlib stream would have to be inflated first
- [x] gradient, hatch, transparency
  - [x] `GRADIENT`, `GRADIENTEX`, `HATCH`, `TRANSPARENT`
  - [ ] `WALLPAPER`, which has a format of its own
  - [ ] `FLOATTRANSPARENT`, which nests a whole metafile
- [x] clipping
  - [x] `CLIPREGION`, `ISECTRECTCLIPREGION`, `ISECTREGIONCLIPREGION`
  - [ ] `MOVECLIPREGION`, which would have to move path data already written
- [x] graphics state stack (`PUSH`/`POP`), restoring only what the push named
- [ ] `EPS` substitute metafile
- [ ] version 1 (pre-`VCLMTF`) files, via `SvmConverter.cxx`
- [x] output is escaped and decoded, so no label can cost the whole image
- [x] every action we skip is logged by name

Anything unimplemented is skipped by the action's own length, so the actions
after it still read. None of it occurs in the corpus.

### Where the drawing is approximate

- A `SQUARE` or `RECT` gradient shrinks a rectangle rather than an ellipse,
  which svg has no gradient for; it comes out as the ellipse closest to it.
- A triple hatch's third line set runs corner to corner of the pattern tile,
  so those lines sit `distance / √2` apart rather than `distance`.

## References

- [`metaact.hxx`](https://github.com/LibreOffice/core/blob/master/include/vcl/metaact.hxx)
  — what each action means.
- [`SvmReader.cxx`](https://github.com/LibreOffice/core/blob/master/vcl/source/filter/svm/SvmReader.cxx)
  — the binary layout, per action, per version. The authority when a field is
  in doubt.
- [`SvmConverter.cxx`](https://github.com/LibreOffice/core/blob/master/vcl/source/filter/svm/SvmConverter.cxx)
  — the version 1 format, converted to the current one on read.
- [`svgwriter.cxx`](https://github.com/LibreOffice/core/blob/master/filter/source/svg/svgwriter.cxx)
  — LibreOffice's own metafile → svg export, i.e. our problem already solved.
  The reference for mapping decisions.
- [`textenc.h`](https://github.com/LibreOffice/core/blob/master/include/rtl/textenc.h)
  — the `rtl_TextEncoding` numbers a font's charset is one of.
- [`SPEC`](https://github.com/ONLYOFFICE/core/blob/master/DesktopEditor/raster/Metafile/StarView/SPEC)
  — ONLYOFFICE's prose write-up, modelled on [MS-WMF]. Cheap to read, but
  incomplete: several FIXMEs, `Color` and the polygon flags unfinished.

### Related work

- https://github.com/SoftarexTechnologies/svmconv
