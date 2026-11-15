# Design

## Principles

- One API for every format. The format specifics stay behind it.
- Public handles share document state; edits are visible through every handle.
- Element IDs are stable. Removed elements and their descendants reject access;
  traversal does not provide a snapshot across edits.
- Saving depends on the recorded changes, not on the internal representation
  of the document.
- Sparse tables are not stored densely in memory.

## Documents

- [Editing design](editing.md): in-browser editing. The browser records an op
  log, and C++ replays it on save. Elements are addressed by stable ids.
- [Document editing design](document-editing.md): the editor of the document
  view. Runs, paragraphs and inline formatting.
- [Spreadsheet editing design](spreadsheet-editing.md): cells edited and
  formatted by position, and formulas computed in the page.
- [Plain-text editing design](txt-editing.md): the editor of the plain-text
  view. One `setContent` op.
- [Lazy loading design](lazy-loading.md): a large file as chunks that the
  browser lays out only near the viewport, and that core renders on demand;
  a search over the chunks that have not loaded, and images that load near
  the viewport.
- [The formula evaluator without exceptions](formula-without-exceptions.md):
  stops as `std::expected` values, so the formula module builds with
  `-fno-exceptions` and loads in any WebView.
- [Document types design](document-types.md): one public handle per document
  type (`Spreadsheet`, `Presentation`, `Drawing`, `TextDocument`), holding
  the methods only that type answers; the old ones on `Document` deprecated.
- [Document creation design](document-creation.md): a new odt, ods, odp,
  docx, xlsx or pptx, generated in code and opened through the read path.
- [Structure editing design](structure-editing.md): sheets and slides
  inserted, removed and moved, slides from a layout, and text boxes.
- [Conversion design](conversion.md): a document turned into another file
  type of its document type, read through the model and written only through
  the edit API.

## Diagrams

- [PDF CJK text: the `code → CID → Unicode` path](pdf-cjk-code-cid-unicode.html):
  why a composite (Type0) font needs the legacy CMap tables, and how the
  `pdf_cid_data` lookup works. Open it in a browser.
