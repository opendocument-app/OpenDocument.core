# OOXML spreadsheet implementation

## General

this implementation relies on [OOXML](../README.md)

## Features

### Functional

- [ ] edit

### Styles

- [x] font
    - [x] size
    - [ ] italic, bold
    - [ ] alignment
    - [ ] underline, strike through
    - [ ] color
    - [ ] family
    - [ ] superscript, subscript
- [ ] links
- [ ] images
- [ ] annotations

## References

- http://officeopenxml.com/anatomyofOOXML-xlsx.php

## Shortcoming

- images are not implemented yet. this need to maintain some references across different XML files
  which should be properly implemented throughout all OOXML
- the navigation between rows and cells was quite tricky and the code is not that nice.
  a document index might solve that problem
