# OOXML spreadsheet implementation

Reader for spreadsheet documents (`.xlsx`).

This implementation relies on [OOXML](../README.md).

The workbook is parsed from `xl/workbook.xml`, with each sheet, the shared
string table and drawings pulled in via relationships (see
`ooxml_spreadsheet_parser.cpp`). Cell styles are resolved from `xl/styles.xml`
through the `cellXfs` / `fonts` / `fills` / `borders` indices (see
`ooxml_spreadsheet_style.cpp`).

## Features

Roughly ordered by importance.

### Functional

- [x] open
- [x] sheets
  - [x] columns, rows, cells
  - [x] dimensions
  - [x] shared strings
  - [x] merged cells (`mergeCells`)
  - [x] shapes / images anchored to a sheet (`xdr:twoCellAnchor`)
  - [x] cell value types (number vs. string; dates/booleans/errors reported as
        string)
  - [ ] computed values (formulas are not evaluated; the cached `<v>` result is
        shown)
- [ ] edit
- [ ] save

### Styles

- [x] font
  - [x] family / name
  - [x] size
  - [x] bold
  - [x] color
  - [ ] italic
  - [ ] underline, strike through
  - [ ] superscript, subscript
- [x] cell
  - [x] background / fill color
  - [x] borders (thin only)
  - [x] alignment (horizontal & vertical center, text rotation)
- [x] images
- [ ] links
- [ ] annotations / comments

## References

- http://officeopenxml.com/anatomyofOOXML-xlsx.php
