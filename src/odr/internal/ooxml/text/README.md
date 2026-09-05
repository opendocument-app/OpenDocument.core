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
  - [x] numbering (`numbering.xml` resolved to markers: `w:numFmt`, `w:lvlText`
    templates, `w:start`, `w:lvlOverride`, `w:numStyleLink`)
  - [ ] a `w:numPr` inherited through `w:pStyle`
  - [ ] picture bullets (`w:lvlPicBulletId`)
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
  - [x] alignment (`start` / `end` stay relative to the direction)
  - [x] base direction (`w:bidi`, on the paragraph and on `w:sectPr`)
  - [x] indentation / left & right margins
  - [x] top / bottom margins, line height (`w:spacing`, incl.
    `w:contextualSpacing`)
  - [ ] `w:beforeLines` / `w:afterLines`, autospacing (the value next to an
    autospacing flag is dropped rather than computed)
  - [ ] `w:lineRule="atLeast"` as a *minimum* — css has no minimum line
    height, so the value is applied as the line height and a line whose font
    is taller than it does not grow the way word grows it
- [x] tables
  - [x] table width
  - [x] cell vertical alignment
  - [x] borders, on the cell (`w:tcBorders`) and on the table
    (`w:tblPr/w:tblBorders`, frame + `w:insideH`/`w:insideV`, lowered onto the
    cells)
  - [ ] word's conflict resolution where two cells meet at a rule — the leading
    cell's own border wins rather than the heavier of the two
  - [x] row height (`w:trHeight`, as a minimum — `w:hRule="exact"` is not)
  - [x] table style reference (`w:tblStyle`, cascading its paragraph, text and
    border properties into the table)
  - [ ] cell width (parsed but not applied)
  - [ ] conditional table formatting (`w:tblStylePr`: banding, first row, …)
- [x] page layout (`w:sectPr`: size, orientation, margins)
  - [ ] one layout per section; the first section's applies to the document
- [x] graphic / drawing styles
  - [x] floating drawings (`wp:anchor`): `wp:positionH`/`wp:positionV` offsets,
    `wp:align` side, `wp:wrapSquare`/`Tight`/`Through`/`TopAndBottom`/`None`
  - [x] `behindDoc`, as a negative z-index
  - [ ] a page-relative origin, and `wp:positionV`'s `wp:align` — a frame is
    kept in the text flow, so neither has an origin to measure against
  - [ ] `relativeHeight` as the stacking order between two drawings
  - [ ] fill and stroke

## References

- http://officeopenxml.com/anatomyofOOXML.php

## Shortcoming

- `<w:rFonts w:asciiTheme="minorHAnsi" w:eastAsiaTheme="minorHAnsi" w:hAnsiTheme="minorHAnsi" w:cstheme="minorBidi"/>`
  is unhandled. example: `Sample large docx.docx`
