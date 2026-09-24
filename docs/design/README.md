# Design

## Principles

- One API for every format. The format specifics stay behind it.
- The public API has value semantics. Handles are immutable.
- Iterators only traverse immutable objects, so nothing invalidates them.
- Saving depends on the recorded changes, not on the internal representation
  of the document.
- Sparse tables are not stored densely in memory.

## Documents

- [Editing design](editing.md): in-browser editing. The browser records an op
  log, and C++ replays it on save. Elements are addressed by stable ids.
- [Document editing design](document-editing.md): the editor of the document
  view. Runs, paragraphs and inline formatting.
- [Spreadsheet editing design](spreadsheet-editing.md): cells edited by
  position, and the formula plan.
- [Plain-text editing design](txt-editing.md): the editor of the plain-text
  view. One `setContent` op.

## Diagrams

- [PDF CJK text: the `code → CID → Unicode` path](pdf-cjk-code-cid-unicode.html):
  why a composite (Type0) font needs the legacy CMap tables, and how the
  `pdf_cid_data` lookup works. Open it in a browser.
