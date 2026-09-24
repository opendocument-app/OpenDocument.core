# OOXML presentation implementation

Reader and editor for presentations (`.pptx`). It relies on
[OOXML](../README.md). The presentation is parsed from
`ppt/presentation.xml`. Each slide named by `p:sldIdLst` is loaded via
relationships, and its shape tree (`p:cSld/p:spTree`) is walked
(`ooxml_presentation_parser.cpp`). Styles resolve inline from the run and
paragraph properties (`ooxml_presentation_style.cpp`).

## Features

### Functional

- [x] open
- [x] slides
  - [x] shapes (`p:sp`), text bodies
  - [x] slide size (`p:sldSz`) and slide names
  - [x] slide background (`p:bg`, inherited from layout and master)
  - [ ] slide master and layout inheritance beyond theme colours and background
- [x] text extraction
- [x] edit (text content, text style, paragraph structure)
- [x] save

### Content

- [x] paragraphs, runs / spans
- [x] line breaks
- [x] tables (`p:graphicFrame` / `a:tbl`: grid columns with widths, rows, cells, merged cells)
- [ ] images
- [ ] listings
- [ ] annotations / comments

### Styles

- [x] font
  - [x] family (`a:latin`)
  - [x] size
  - [x] italic, bold
  - [x] underline, strike through
  - [x] color, background (highlight), theme colours (`a:schemeClr`; no `a:lumMod` / `a:tint` / `a:shade` transforms)
  - [x] shadow
  - [x] superscript, subscript (`@baseline`)
- [x] paragraph
  - [x] alignment (`@algn`)
  - [x] base direction (`@rtl`)
  - [x] left and right margins (`@marL` / `@marR`)
  - [x] line height (`a:lnSpc`), space before and after (`a:spcPts` only)
- [x] shape fill (`p:spPr/a:solidFill`) and text anchor (`a:bodyPr/@anchor`)
- [x] tables (column widths, row heights; no `a:tcPr` cell styles)
- [x] page layout (slide size)
- [ ] graphic / drawing styles

## References

- http://officeopenxml.com/anatomyofOOXML-pptx.php
