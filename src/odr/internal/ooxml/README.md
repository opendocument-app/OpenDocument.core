# OOXML implementation

Reader and editor for Office Open XML files: `.docx`, `.pptx` and `.xlsx`.
It relies on [ZIP](../zip/README.md) and, for the encryption container,
[CFB](../cfb/README.md).

The three formats share almost nothing beyond packaging and encryption. The
per-format checklists are [DOCX](text/README.md), [PPTX](presentation/README.md)
and [XLSX](spreadsheet/README.md).

## Features

- [x] open
  - [x] decryption
    - [x] ECMA-376 standard encryption (AES + SHA1)
    - [ ] agile encryption (rejected with `MsUnsupportedCryptoAlgorithm`)
    - [ ] extensible encryption
    - [ ] big endian hosts
- [x] meta data
  - [x] file type detection (docx / pptx / xlsx, encrypted package)
  - [ ] document statistics (page / table count)

| Capability | DOCX | PPTX | XLSX |
|---|:-:|:-:|:-:|
| read | ✓ | ✓ | ✓ |
| styles | ✓ | ✓ | ✓ |
| edit text | ✓ | ✓ | |
| edit cell values | | | ✓ |
| save | ✓ | ✓ | ✓ |

## References

- http://officeopenxml.com/
- http://officeopenxml.com/drwOverview.php
- https://github.com/nolze/msoffcrypto-tool
- https://github.com/microsoft/compoundfilereader
