# ODF implementation

Reader, style resolver and partial editor for OpenDocument files: `.odt`,
`.odp`, `.ods`, `.odg`, their template, flat-xml and legacy StarOffice
variants. Design and open work are in [`AGENTS.md`](AGENTS.md). Relies on
[ZIP](../zip/README.md) and [SVM](../svm/README.md).

## Features

### Core (all document types)

- [x] open
  - [x] decryption
    - [x] algorithms: AES256-CBC, AES256-GCM, Triple-DES-CBC, Blowfish-CFB
    - [x] key derivation: PBKDF2, Argon2id
    - [x] start key / checksum: SHA1, SHA256, and the 1K variants
    - [x] per-file and single `encrypted-package` layouts
- [x] meta data
  - [x] file type detection, including templates and StarOffice mimetypes
  - [x] document statistics (page and table count)
- [x] text extraction
  - [x] spaces (`text:s`), tabs (`text:tab`), line breaks
- [x] save
  - [ ] encryption on save
- [x] edit
  - [x] text content
  - [x] text style (weight, style, underline, line-through, size, colour,
    background)
  - [x] structural edits: insert text, split, merge and insert paragraphs,
    remove elements
  - [ ] paragraph style, page and drawing attributes

### Styles (all document types)

- [x] font
  - [x] family / name (resolved against `style:font-face`)
  - [x] size (absolute and percentage)
  - [x] italic, bold
  - [x] underline, strike through
  - [x] color, background color
  - [x] shadow
  - [x] superscript, subscript (`style:text-position`, with the relative font
    size)
- [x] paragraph
  - [x] alignment (`start` / `end` are absolute here, unlike `w:jc`)
  - [x] base direction (`style:writing-mode`; the vertical modes are not laid
    out vertically)
  - [x] margins (a percentage is of the parent style's margin)
  - [x] line height (absolute and percentage)
  - [x] first line indent (`fo:text-indent`)
- [x] links
- [x] images
  - [x] internal and external references, `office:binary-data`
  - [x] svm
- [x] embedded objects (`draw:object`)
  - [x] charts (`chart:bar`, `line`, `area`, `scatter`, `circle`, `ring`),
    drawn from the chart part's own `local-table` #179
  - [ ] stacked and percentage plots, secondary axes, trend lines, data labels
  - [ ] the axis number format (a date axis shows its serial number)
  - [ ] `draw:object-ole` (an OLE blob, not ODF markup)
  - [x] anything else falls back to the `draw:image` replacement
- [x] tables
  - [x] column width, row height, table width
  - [x] cell vertical alignment, background, padding, borders
  - [x] cell / column / row repetition
  - [x] column and row spans, covered cells
- [x] drawings / shapes
  - [x] frame, group (`draw:g`), text box
  - [x] line, rect, circle, ellipse
  - [x] path, polygon, polyline, regular polygon, connector (drawn from the
    `svg:d` the producer wrote)
  - [x] measure (`draw:measure`, with its `text:measure` label)
  - [x] caption (`draw:caption`; the box, not the tail)
  - [ ] 3-D scene (`dr3d:scene`)
  - [x] custom shapes (bounding box, fill/stroke) #159
    - [x] enhanced geometry (`draw:enhanced-path`, `draw:equation`,
      `draw:modifiers`, `draw:mirror-horizontal` / `-vertical`)
    - [ ] a `draw:type` preset with no `draw:enhanced-path` (needs
      LibreOffice's preset table; every file in the corpus writes the path)
    - [ ] `draw:text-areas` (text is laid out in the whole box)
    - [ ] `draw:handle` (the interactive control points)
    - [ ] `F` / `S`: one subpath painted differently from the rest
  - [x] graphic style: stroke width/color, fill color, vertical align, text wrap
  - [ ] gradient and hatch fills, `draw:opacity` / `draw:opacity-name`, and the
    dash a `draw:stroke` names
  - [ ] arrowheads (`draw:marker`, `draw:marker-start` / `-end`)
  - [x] transform (`draw:transform`, its operation list composed to one matrix)
  - [ ] mirror (`style:mirror`, and `draw:mirror-*` on a shape with no
    enhanced geometry)
- [x] page layout (size, orientation, margins, base direction)
- [ ] annotations (`office:annotation`)

### Text documents (`.odt`)

- [x] headings and paragraphs
- [x] spans
- [x] bookmarks
- [x] sections, date / time fields (rendered as generic groups)
- [x] table of contents / indices (rendered as generic groups)
- [x] soft page breaks
- [x] listings
  - [x] bullets
  - [x] numbering (`text:list-style` levels resolved to markers: format,
    prefix / suffix, `text:display-levels`, `text:start-value`,
    `text:continue-numbering`)
  - [ ] `text:list-level-style-image` (falls back to a bullet)
  - [ ] `text:outline-style` (indexed, not applied to headings)

### Spreadsheet documents (`.ods`)

- [x] sheets
  - [x] dimensions, content range detection
  - [x] cell value types (float, boolean, date, time, string)
  - [x] cell values (`office:value`, and `table:formula` as its own string)
  - [x] shapes anchored to a sheet
  - [ ] computed values (formulas are read, not evaluated)
- [x] edit
  - [x] cell values (number, string, cleared)
  - [x] a repeated cell, by cutting the run around the position written
  - [x] a cell the file states no element for
  - [x] a cell past the last row or column
  - [ ] a formula cell, and a cell of richer markup than one plain paragraph

### Presentation documents (`.odp`)

- [x] slides (`draw:page`), slide names
- [x] slide master page (`style:master-page`)
- [x] paging (e.g. style slides, center)

### Graphics / drawing documents (`.odg`)

- [x] pages (`draw:page`), page names
- [x] page master page
- [x] shapes (see drawings / shapes above)

## References

- https://www.openoffice.org/framework/documentation/mimetypes/mimetypes.html
- http://docs.oasis-open.org/office/v1.2/os/OpenDocument-v1.2-os-part1.html
- custom shapes: https://wiki.openoffice.org/wiki/Create_a_New_Custom_Shape_in_Source_in_File#Features_in_Detail
- AES-GCM encryption: https://www.w3.org/TR/xmlenc-core1/#sec-AES-GCM
- https://ringlord.com/odfdecrypt.html
