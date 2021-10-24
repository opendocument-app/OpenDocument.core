# OOXML text implementation

## General

this implementation relies on [OOXML](../README.md)

## Features

### Styles

- [x] font
  - [x] size
  - [x] italic, bold
  - [x] alignment
  - [x] underline, strike through
  - [x] color
  - [x] family
  - [ ] superscript, subscript
- [x] links
- [x] tables
- [x] images
  - [x] svm
- [x] listings
  - [x] bullets
  - [ ] numbering
- [ ] annotations
- [x] page layout

## References

- http://officeopenxml.com/anatomyofOOXML.php

## Shortcoming

- `<w:rFonts w:asciiTheme="minorHAnsi" w:eastAsiaTheme="minorHAnsi" w:hAnsiTheme="minorHAnsi" w:cstheme="minorBidi"/>`
  is unhandled. example: `Sample large docx.docx`
