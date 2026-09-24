# OOXML spreadsheet implementation

Reader and editor for spreadsheets (`.xlsx`). It relies on
[OOXML](../README.md). The workbook is parsed from `xl/workbook.xml`. Each
sheet, the shared strings and the drawings are loaded via relationships
(`ooxml_spreadsheet_parser.cpp`). Cell styles resolve from `xl/styles.xml`
through the `cellXfs`, `fonts`, `fills` and `borders` indices
(`ooxml_spreadsheet_style.cpp`).

## Features

### Functional

- [x] open
- [x] sheets
  - [x] columns, rows, cells
  - [x] dimensions
  - [x] shared strings
  - [x] inline strings (`t="inlineStr"`)
  - [x] merged cells (`mergeCells`)
  - [x] shapes and images anchored to a sheet (`xdr:twoCellAnchor`)
  - [x] cell value types (number, string, boolean, error, date)
  - [x] cell values (`<v>` as a number, `<f>` as its own string, shared formulas)
  - [ ] computed values (the cached `<v>` result is shown)
  - [ ] number formats (a date shows as its serial)
- [x] edit
  - [x] cell values (number, string, boolean, cleared); a written string goes inline
  - [x] a cell the file states no `c` for
  - [ ] a covered cell, a formula cell, a date, time or error value
- [x] save

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
  - [x] borders (`0.75pt solid` only)
  - [x] alignment (horizontal, vertical, text rotation)
  - [ ] protection
- [x] images
- [ ] links
- [ ] annotations / comments

## References

- http://officeopenxml.com/anatomyofOOXML-xlsx.php
