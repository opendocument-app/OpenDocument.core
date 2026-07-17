# OOXML text implementation

Reader and editor for word processing documents (`.docx`).

This implementation relies on [OOXML](../README.md).

The document tree is parsed from `word/document.xml` (see
`ooxml_text_parser.cpp`); styles are resolved from `word/styles.xml` over the
`w:basedOn` parent hierarchy and `w:docDefaults` (see `ooxml_text_style.cpp`).

## Features

Roughly ordered by importance.

### Functional

- [x] open
- [x] text extraction (`w:t`, tabs)
- [x] edit
  - [x] text content
  - [ ] structural edits (insert / delete elements)
- [x] save
  - [ ] encryption (re-encrypting on save is unsupported)

### Content

- [x] headings and paragraphs
- [x] runs / spans
- [x] line breaks
- [x] hyperlinks
- [x] bookmarks
- [x] tables (grid columns, rows, cells, merged cells via
      `w:gridSpan`/`w:vMerge`)
- [x] images (`w:drawing`)
- [x] structured document tags (rendered as generic groups)
- [x] listings
  - [x] bullets (incl. nesting by level)
  - [ ] numbering (`w:numPr` levels are honored, but `numbering.xml` formats are
    not resolved to actual numbers)
- [ ] annotations / comments

### Styles

- [x] font
  - [x] family (`w:rFonts`)
  - [x] size
  - [x] italic, bold
  - [x] underline, strike through
  - [x] color, background (highlight)
  - [x] shadow
  - [ ] superscript, subscript
- [x] paragraph
  - [x] alignment
  - [x] indentation / left & right margins
  - [ ] top / bottom margins, line height
- [x] tables
  - [x] table width
  - [x] cell vertical alignment, borders
  - [ ] cell width (parsed but not applied)
  - [ ] table row styles
- [x] page layout (via master page)
- [ ] graphic / drawing styles

## References

- http://officeopenxml.com/anatomyofOOXML.php

## Shortcoming

- `<w:rFonts w:asciiTheme="minorHAnsi" w:eastAsiaTheme="minorHAnsi" w:hAnsiTheme="minorHAnsi" w:cstheme="minorBidi"/>`
  is unhandled. example: `Sample large docx.docx`
