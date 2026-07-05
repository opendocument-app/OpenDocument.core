# Design

## Diagrams

- [PDF CJK text: the `code → CID → Unicode` path](pdf-cjk-code-cid-unicode.html)
  — why composite (Type0) fonts need the legacy-CMap tables and how the
  `pdf_cid_data` lookup works (open in a browser).

## Motivation

- non-intrusive read/write access to documents
  - saving should not depend on our internal representation of the document but only the changes
- support of different formats thought one API
- lightweight API
- handle document specifics behind the API
- value semantics for the user facing API

## Document abstraction

## Spreadsheet abstraction

- inefficient to store sparse tables densely in-memory

## Iterators

- try to use iterators only for immutable objects
  - avoids iterator invalidation, multithreading issues

## Future

### Breaking API changes (next major)

- `Logger` should be a value type, like the other interface types.
  Eases lifetime management; consistent with value semantics for the user-facing API.
- Collapse `FileMeta` into a single type. The nested `DocumentMeta`
  does not earn the extra indirection.
- drop `wvWare` (the alternative `.doc` decoder, `oldms_wvware/`)
- drop `pdf2htmlEX` (the poppler/pdf2htmlEX PDF path)

### Document index

- replace iterators where it makes sense with an index
- challenges
  - some document formats "compress" information (e.g. ods columns/rows/cells or xlsx strings).
    these elements need to be inflated before indexing to guarantee correctness
    (counter example: editing a cell that is repeated would destroy the other cells index)

### Spreadsheet handling

- rework sheet handling by mapping IDs directly to columns, rows, cells

### New format support

- markdown (candidates: [md4c](https://github.com/mity/md4c), [cmark](https://github.com/commonmark/cmark))
- xml
- json
- csv
- plain text with optional line numbers

### Language bindings

- python
- jni

### Internal refactoring

- elevate the pdf stream helpers into utils
  - free functions may fit better than a class
  - decide whether to build on `istream` or `streambuf`
- move the crypto utils into utils
- adopt the logger downstream, including the translation path

### Open tasks

- collect additional pdf files via the translate cli and capture the ones that break
- exercise editing across all formats (odp editing appears broken via an HTML issue)
