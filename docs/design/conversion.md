# Document conversion design

`odr::convert` turns a document into one of another file type with the same
document type: docx to odt, xls to xlsx, csv to ods. It reads the source
through the public model and writes the target only through the edit API,
starting from [`create_document`](document-creation.md).

## Why the edit API

A writer per target format would interpret each format a second time. The
odf and ooxml engines already know how to write a paragraph, a cell or a
style, and the editor tests them against LibreOffice and Word. A converter
with its own writers would drift from them.

So the converter has no writer of its own: it walks the source and, for each
element, calls the method that makes it. Everything it needs and the edit API
lacks becomes an edit method first, which the editor then has too.

The cost is that a conversion carries only what the edit API can write. That
is the contract. It reports what it leaves out, and its fidelity grows with
editing.

## API

```cpp
enum class ConversionLossKind {
  dropped,  ///< the element and its subtree were not written
  restyled, ///< written, without some fields of its style
};

struct ConversionLoss final {
  ElementIdentifier element; ///< in the source
  ConversionLossKind kind;
};

/// What @ref convert made, and what of the source it left out.
class Conversion final {
public:
  [[nodiscard]] const Document &document() const noexcept;
  /// In reading order; a dropped subtree is named by its root only.
  [[nodiscard]] const std::vector<ConversionLoss> &losses() const noexcept;
};

[[nodiscard]] Conversion convert(const Document &source, FileType target,
                                 const Logger &logger = Logger::null());
```

- The target is a `FileType` that `FileTypeCapabilities::create` lists:
  odt, ods, odp, docx, xlsx, pptx. Any other throws `UnsupportedFileType`.
- The source is any `Document`: that of a `DocumentFile`, and
  `CsvFile::document()` and `MarkdownFile::document()`. A pdf is no
  `Document`, so it is no source.
- The document types must match, else `WrongDocumentType`. Text to
  spreadsheet is not a conversion.
- The result is in memory and unsaved, as `create_document`'s is. The source
  does not change.
- A target of the source's own file type is allowed and is not a copy: it
  goes through the walk. The tests use it.

## The walk

The walk lives in `src/odr/internal/convert/`, one file per document type.
It includes the public headers and `internal/formula`, never an engine.

- **Start from the blank.** What the new document starts with is reused
  (its paragraph) or removed at the end (`Sheet1`, the title slide), after the
  source's content is in, since a workbook refuses to lose its last sheet.
- **Styles are resolved.** The reader gives each element its resolved style.
  The walk makes the element, reads its style back from the target, and
  states the fields that differ. A span adds no element: its runs carry its
  style in theirs.
- **What cannot be written is a loss, not a failure.** An element type the
  edit API cannot make is `dropped`. A style field no edit method writes is
  `restyled`. The walk keeps one table of the fields each edit method
  writes, and a test sets every listed field through every target engine, so
  the table cannot claim a field an engine refuses.
- **Sparse stays sparse.** A sheet is walked over `Sheet::content()`, never
  its full dimensions.

### Spreadsheets

- Each sheet: `Spreadsheet::insert_sheet(index, name)`. A name the target
  refuses fails the conversion with `std::invalid_argument`. Renaming it
  would mean rewriting every formula that names it, which is the sheet
  rename of [structure editing](structure-editing.md#open-work).
- Each cell: `Sheet::set_cell` with the source's `CellValue`. A formula is
  parsed in the source's syntax and written in the target's with
  `formula::to_string`. The save recalculates it.
- Each cell's style: `Sheet::set_cell_style`, today fill, alignment and the
  run keys.

### Text documents

- Each paragraph: the blank's first, then `insert_paragraph_after`, styled
  with `Paragraph::set_style`, today alignment only.
- Each run: `append_text` on the paragraph, styled with `Text::set_style`,
  today the seven inline keys of
  [document editing](document-editing.md#inline-formatting).

### Presentations

- Each slide: `Presentation::insert_slide` from the target's `Blank` layout.
- Each frame that holds text: `insert_text_box` at the source frame's
  position and size, filled as a text document's paragraphs are.

### Drawings

None: `create_document` makes no odg yet.

## What the edit API needs

Each item is an edit method first, written by every target engine and tested
on its own, and then one line of the walk. A method is C++ only; it gets an
op in the envelope when the editor wants the gesture.

Spreadsheets, first, because their model is the most complete and a reader
expects the least layout of them:

- Function names spelled for the target. `formula::to_string` keeps a name
  as the source spells it, so `_xlfn.CONCAT` would reach an ods for
  `COM.MICROSOFT.CONCAT`. Not an edit method, but the same rule: it goes
  into the formula writer, where the editor's formulas get it too.
- `set_cell` keeping a result stated beside a formula, so a formula the
  evaluator cannot answer still shows the source's cached result.
- Column widths and row heights.
- Merged cells.
- Number formats.
- Borders, padding, wrapping and text rotation in `set_cell_style`.
- Hidden rows and columns, freeze panes, the page layout.

Text documents:

- Font name, superscript and subscript in `Text::set_style`.
- Margins, indent, line height and breaks in `Paragraph::set_style`.
- Heading levels. Without them a heading converts as a formatted paragraph,
  and the target's outline and table of contents lose it.
- Line breaks and tabs, links and bookmarks.
- Lists, tables, frames and images.
- The page layout, master pages, headers and footers.
- A document default text style, so the walk need not state the source's
  font and size on every run.

Presentations:

- The slide size. Until it is written, a 4:3 source puts its frames on a
  16:9 page as stated.
- Images and shapes, backgrounds and masters.
- Placeholders: a title converts as a text box, and the target's outline
  loses it.
- Speaker notes.

What the reader does not model yet is not in the walk either: footnotes,
comments, fields, sections, charts, tracked changes. The reader comes first.

## Validation

1. The corpus: every file in `test/data/input` that opens as a `Document`,
   to every target of its type. Each converts, saves and reopens.
2. To its own type: the HTML of the source and of the converted file
   compared with `htmlcmp`. Each difference is a reported loss.
3. Each target opens in LibreOffice and converts to pdf there. docx, xlsx
   and pptx pass `OpenXmlValidator`.

## Surfaces

- The CLI: `convert <input> <output>`, the target by the output's
  extension, printing the losses.
- Bindings after core: `pyodr.convert`, Java `Odr.convert`, Swift
  `Document.convert(to:)`, npm `odr.convert`.
- The apps offer "Save as" with every `create` target of the document's
  type, and say what was left out.

## Open work

- A csv target, once `create_document` makes an empty csv: `set_cell` writes
  one already, and a formula goes in as its result.
- odg, with odg creation.
