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
  - [x] slide size (`p:sldSz`) and slide names
  - [ ] slide master / layout inheritance
- [x] text extraction
- [ ] edit
- [ ] save

### Content

- [x] paragraphs, runs / spans
- [x] line breaks
- [x] tables (`p:graphicFrame` / `a:tbl`: grid columns incl. widths, rows,
      cells, merged cells)
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
- [x] tables (column widths, row heights; no `a:tcPr` cell styles)
- [x] page layout (slide size)
- [ ] graphic / drawing styles

## References

- http://officeopenxml.com/anatomyofOOXML-pptx.php
