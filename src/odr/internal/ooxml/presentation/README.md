# OOXML presentation implementation

Reader for presentation documents (`.pptx`).

This implementation relies on [OOXML](../README.md).

The presentation is parsed from `ppt/presentation.xml`, with each slide pulled in
via relationships and its shape tree (`p:cSld` / `p:spTree`) walked for content
(see `ooxml_presentation_parser.cpp`). Text and paragraph styles are resolved
inline from the run / paragraph properties (see
`ooxml_presentation_document.cpp`).

## Features

Roughly ordered by importance.

### Functional

- [x] open
- [x] slides
  - [x] shapes (`p:sp`), text bodies
  - [x] slide master page / page layout
- [x] text extraction
- [ ] edit
- [ ] save

### Content

- [x] paragraphs, runs / spans
- [x] line breaks
- [x] tables (grid columns, rows, cells)
- [ ] images
- [ ] listings
- [ ] annotations / comments

### Styles

- [x] font
  - [x] family (`rFonts`)
  - [x] size
  - [x] italic, bold
  - [x] underline, strike through
  - [x] color, background (highlight)
  - [x] shadow
  - [ ] superscript, subscript
- [x] paragraph
  - [x] alignment
  - [x] indentation / left & right margins
- [x] tables
- [x] page layout (via master page)
- [ ] graphic / drawing styles

## References

- http://officeopenxml.com/anatomyofOOXML-pptx.php
