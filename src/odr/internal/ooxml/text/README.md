# OOXML text implementation

Reader and editor for word processing documents (`.docx`). It relies on
[OOXML](../README.md). The tree is parsed from `word/document.xml`
(`ooxml_text_parser.cpp`). Styles resolve from `word/styles.xml` over the
`w:basedOn` hierarchy and `w:docDefaults` (`ooxml_text_style.cpp`).

## Features

### Functional

- [x] open
- [x] text extraction (`w:t`, tabs)
- [x] edit
  - [x] text content and text style
  - [x] structural edits (insert text, split and merge paragraphs, remove elements)
- [x] save
  - [ ] encryption on save

### Content

- [x] headings and paragraphs
- [x] runs / spans
- [x] line breaks
- [x] hyperlinks
- [x] bookmarks
- [x] tables (grid columns, rows, cells, merged cells via `w:gridSpan` / `w:vMerge`)
- [x] images (`w:drawing`)
- [x] structured document tags (as generic groups)
- [x] listings
  - [x] bullets, nested by level
  - [x] numbering (`numbering.xml`: `w:numFmt`, `w:lvlText`, `w:start`, `w:lvlOverride`, `w:numStyleLink`)
  - [ ] a `w:numPr` inherited through `w:pStyle`
  - [ ] picture bullets (`w:lvlPicBulletId`)
- [ ] annotations / comments

### Styles

- [x] font
  - [x] family (`w:rFonts`, literal names only; theme fonts such as `w:asciiTheme` are ignored)
  - [x] size
  - [x] italic, bold
  - [x] underline, strike through
  - [x] color, background (highlight and `w:shd`)
  - [x] shadow
  - [ ] superscript, subscript
- [x] paragraph
  - [x] alignment (`start` / `end` stay relative to the direction)
  - [x] base direction (`w:bidi`, on the paragraph and on `w:sectPr`)
  - [x] indentation, left and right margins
  - [x] top and bottom margins, line height (`w:spacing`, `w:contextualSpacing`)
  - [ ] `w:beforeLines` / `w:afterLines`, autospacing
  - [ ] `w:lineRule="atLeast"` as a minimum (applied as a fixed line height)
- [x] tables
  - [x] table width
  - [x] cell vertical alignment
  - [x] borders on the cell (`w:tcBorders`) and on the table (`w:tblBorders`, lowered onto the cells)
  - [ ] Word's conflict resolution where two cells meet (the leading cell wins, not the heavier border)
  - [x] row height (`w:trHeight` as a minimum; `w:hRule="exact"` is not)
  - [x] table style reference (`w:tblStyle`)
  - [ ] cell width (`w:tcW` is read and dropped)
  - [ ] conditional table formatting (`w:tblStylePr`)
- [x] page layout (`w:sectPr`: size, orientation, margins)
  - [ ] one layout per section (the first section's applies to the document)
- [x] drawings
  - [x] floating drawings (`wp:anchor`): `wp:positionH` / `wp:positionV` offsets, `wp:align` side, `wp:wrapSquare` / `Tight` / `Through` / `TopAndBottom` / `None`
  - [x] `behindDoc`, as a negative z-index
  - [ ] a page-relative origin, and `wp:positionV`'s `wp:align` (a frame stays in the text flow)
  - [ ] `relativeHeight` as the stacking order between two drawings
  - [ ] fill and stroke

## References

- http://officeopenxml.com/anatomyofOOXML.php
