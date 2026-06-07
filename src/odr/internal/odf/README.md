# ODF implementation

Reader, style resolver and (partial) editor for OpenDocument files: text
(`.odt`), presentation (`.odp`), spreadsheet (`.ods`) and graphics/drawing
(`.odg`), including their template and legacy StarOffice variants.

This implementation relies on [ZIP](../zip/README.md) and [SVM](../svm/README.md)

The document tree is parsed from `content.xml` (see `odf_parser.cpp`) into an
`ElementRegistry`. Styles from `styles.xml` and the automatic/content styles are
resolved lazily on top of the family/parent hierarchy (see `odf_style.cpp`).
Most features below are shared by all document types; the ones that only make
sense for a single type are split out into their own sections.

## Features

Roughly ordered by importance.

### Core (all document types)

- [x] open
  - [x] decryption
    - [x] algorithms: AES256-CBC, Triple-DES-CBC, Blowfish-CFB, AES256-GCM
    - [x] key derivation: PBKDF2, Argon2id
    - [x] start key / checksum: SHA1, SHA256 (incl. 1K variants)
    - [x] per-file and single `encrypted-package` layouts
- [x] meta data
  - [x] file type detection (incl. templates and legacy StarOffice mimetypes)
  - [x] document statistics (page / table count)
- [x] text extraction
  - [x] spaces (`text:s`), tabs (`text:tab`), line breaks
- [x] save
  - [ ] encryption (re-encrypting on save is unsupported)
- [x] edit
  - [x] text content
  - [ ] structural edits (insert / delete elements)

### Styles (all document types)

- [x] font
  - [x] family / name (resolved against `style:font-face`)
  - [x] size (absolute and percentage)
  - [x] italic, bold
  - [x] underline, strike through
  - [x] color, background color
  - [x] shadow
  - [ ] superscript, subscript (`style:text-position`)
- [x] paragraph
  - [x] alignment
  - [x] margins
  - [x] line height
- [x] links
- [x] images
  - [x] internal and external references
  - [x] svm
- [x] tables
  - [x] column width, row height, table width
  - [x] cell vertical alignment, background, padding, borders
  - [x] cell / column / row repetition
  - [x] column and row spans, covered cells
- [x] drawings / shapes
  - [x] frame, group (`draw:g`), text box
  - [x] line, rect, circle
  - [x] custom shapes (bounding box, fill/stroke) #159
    - [ ] enhanced geometry / shape path rendering
  - [x] graphic style: stroke width/color, fill color, vertical align, text wrap
  - [ ] transform (e.g. flip, rotate)
- [x] page layout (size, orientation, margins)
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
  - [ ] numbering (`style:list-style` / `style:outline-style` are indexed but
    not yet rendered as numbers)

### Spreadsheet documents (`.ods`)

- [x] sheets
  - [x] dimensions, content range detection
  - [x] cell value types (float, string)
  - [x] shapes anchored to a sheet
  - [ ] computed values (stored values are used as-is; formulas are not
    evaluated)
- [ ] edit (currently disabled, see `Document::is_editable`)

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
- custom shapes
  - https://wiki.openoffice.org/wiki/Create_a_New_Custom_Shape_in_Source_in_File#Features_in_Detail
- Recent encryption https://www.w3.org/TR/xmlenc-core1/#sec-AES-GCM

### Related work

- https://ringlord.com/odfdecrypt.html
