# OOXML implementation

Reader and (partial) editor for Office Open XML files: word processing
(`.docx`), presentation (`.pptx`) and spreadsheet (`.xlsx`).

This implementation relies on [ZIP](../zip/README.md) and [CFB](../cfb/README.md)
(the latter for the encryption container).

The three formats share almost nothing beyond packaging and encryption, so most
features are documented in the per-format READMEs: [DOCX](text/README.md),
[PPTX](presentation/README.md) and [XLSX](spreadsheet/README.md). This file
covers only what is common to all three.

## Features

Roughly ordered by importance.

### Shared

- [x] open
  - [x] decryption
    - [x] ECMA-376 _standard_ encryption (AES + SHA1)
    - [ ] _agile_ encryption (rejected with `MsUnsupportedCryptoAlgorithm`)
    - [ ] _extensible_ encryption
    - [ ] big endian hosts
- [x] meta data
  - [x] file type detection (docx / pptx / xlsx, encrypted package)
  - [ ] document statistics (page / table count)

### Per-format capabilities

| Capability  | DOCX | PPTX | XLSX |
| ----------- | :--: | :--: | :--: |
| read        |  ✓   |  ✓   |  ✓   |
| styles      |  ✓   |  ✓   |  ✓   |
| edit text   |  ✓   |      |      |
| save        |  ✓   |      |      |

See the per-format READMEs for the detailed element and style coverage.

## References

- http://officeopenxml.com/
- http://officeopenxml.com/drwOverview.php

### Related work

- https://github.com/nolze/msoffcrypto-tool
- https://github.com/microsoft/compoundfilereader
