# Design

## Documents

- [Editing design](editing.md) — architecture for in-browser editing of ODF/OOXML:
  fat-browser op log replayed on save, stable element ids, and a preliminary
  implementation plan.
- [Spreadsheet editing design](spreadsheet-editing.md) — cells edited by
  position through the same op log, a browser-side editing mode with refusal
  feedback, and formulas recomputed once, in C++; staged from number/string
  cells to a formula engine.

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

### Document index

- replace iterators where it makes sense with an index
- challenges
  - some document formats "compress" information (e.g. ods columns/rows/cells or xlsx strings).
    these elements need to be inflated before indexing to guarantee correctness
    (counter example: editing a cell that is repeated would destroy the other cells index)

### Spreadsheet handling

- rework sheet handling by mapping IDs directly to columns, rows, cells

### New format support

- xml
- json
- csv
- plain text with optional line numbers
- RTF
- Apple Pages
- Google Docs (gdocs)

### Visualization

- xml / json visualization
- markdown visualization
- open question: is markdown editing just plain-text editing, or does it warrant
  a structured editor?

### Editing

- advanced editing for at least ODF and OOXML:
  - text changes
  - removal of content
  - adding paragraphs
  - changing formatting: bold, italic, underline, highlight, font size
- plain text (txt) editing
- spreadsheets: recompute cell values whose content has functions attached
  - open question: do this only offline (in `odr.core`), or also online (in JS)?

### Annotation

- general highlighting for documents, including PDF
- general handwriting note-taking for documents, including PDF

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

- nothing looks for a data directory any more, and nothing accepts a path to
  one: `GlobalParams` and the libmagic and asset build options are gone with
  v7. What decides where the compiled-in css and js *land* is live and stays —
  `HtmlConfig::embed_shipped_resources`, `resource_path`,
  `relative_resource_paths`, `HtmlResource::is_shipped`.
- collect additional pdf files via the translate cli and capture the ones that break
- exercise editing across all formats (odp editing appears broken via an HTML issue)
