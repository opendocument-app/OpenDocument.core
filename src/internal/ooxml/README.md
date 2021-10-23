# OOXML implementation

## General

this implementation relies on [ZIP](../zip/README.md) and [CFB](../cfb/README.md)

used by [DOCX](text/README.md), [PPTX](presentation/README.md) and [XLSX](spreadsheet/README.md)

docx, pptx and xlsx have almost nothing in common.

## Features

### Functional

- [x] open
    - [x] decryption (only _standard_)
- [x] meta data
- [x] edit
    - [x] text
- [x] save
    - [ ] encryption

## References

- http://officeopenxml.com/
- http://officeopenxml.com/drwOverview.php

### Related work

- https://github.com/nolze/msoffcrypto-tool
- https://github.com/microsoft/compoundfilereader
