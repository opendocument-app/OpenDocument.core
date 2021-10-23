# ODF implementation

## General

this implementation relies on [ZIP](../zip/README.md)

## Features

### Functional

- [x] open
    - [x] decryption
- [x] meta data
- [x] edit
    - [x] text
- [x] save
    - [ ] encryption

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
- [x] drawings
    - [x] line, rect, circle
    - [ ] custom shapes support odf custom-shape #159
    - [ ] transform (e.g. flip, rotate)
- [x] odp slide master / odg page master
- [ ] annotations
- [x] paging (e.g. style slides, center)

## References

- https://www.openoffice.org/framework/documentation/mimetypes/mimetypes.html
- http://docs.oasis-open.org/office/v1.2/os/OpenDocument-v1.2-os-part1.html
- custom shapes
    - https://wiki.openoffice.org/wiki/Create_a_New_Custom_Shape_in_Source_in_File#Features_in_Detail

### Related work

- https://ringlord.com/odfdecrypt.html
