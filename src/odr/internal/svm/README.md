# SVM implementation

StarView Metafile → SVG. See [`AGENTS.md`](AGENTS.md) for how the module is
built and [`PLAN.md`](PLAN.md) for the order the gaps below get closed in.

## Features

- [x] shapes
  - [x] rectangle
  - [x] polyline, polygon, poly-polygon
  - [ ] fill rule of a poly-polygon (holes are painted over)
  - [ ] pixel, point, line, rounded rectangle, ellipse, arc, pie, chord
  - [ ] bézier segments (the polygon flags are not read)
- [x] colour
  - [x] line, fill, text
  - [ ] `LineInfo` (width, dash, join, cap)
  - [ ] text fill, overline (read into the state, never drawn)
- [ ] font
  - [x] size (in map-mode units, which the transform does not apply)
  - [ ] italic, bold
  - [ ] alignment
  - [ ] underline, strike through
  - [x] colour
  - [x] family
- [x] text
  - [x] `TEXT`, `TEXTARRAY`, `STRETCHTEXT` as plain text at a point
  - [ ] the `TEXTARRAY` dx array, `STRETCHTEXT` width, `TEXTRECT`
  - [ ] non-`UCS2` encodings (the bytes go out undecoded, see below)
- [ ] transform (e.g. flip, rotate)
  - [x] map mode origin and scale
  - [ ] map mode unit
- [ ] images (`BMP`, `BMPEX`, `MASK` and their scale/part variants)
- [ ] gradient, hatch, wallpaper
- [ ] clipping regions
- [ ] transparency (`TRANSPARENT`, `FLOATTRANSPARENT`)
- [ ] graphics state stack (`PUSH`/`POP`)
- [ ] `EPS` substitute metafile
- [ ] version 1 (pre-`VCLMTF`) files
- [x] output is escaped, so a `&` in a label cannot cost the whole image
- [x] every action we skip is logged by name

Anything not implemented is skipped by the action's own length, so the actions
after it still read.

### Known defect

Text in a non-`UCS2` encoding is passed through as the bytes the file holds
(`read_ascii_string`). A latin-1 label therefore emits invalid utf-8, and an
xml parser refuses that exactly as hard as an unescaped `&`. Escaping alone
does not make every label safe.

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
- [`SPEC`](https://github.com/ONLYOFFICE/core/blob/master/DesktopEditor/raster/Metafile/StarView/SPEC)
  — ONLYOFFICE's prose write-up, modelled on [MS-WMF]. Cheap to read, but
  incomplete: several FIXMEs, `Color` and the polygon flags unfinished.

### Related work

- https://github.com/SoftarexTechnologies/svmconv
