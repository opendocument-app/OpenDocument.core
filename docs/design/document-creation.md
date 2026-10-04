# Document creation design

How the library makes a new document. A new document is an ordinary
`Document`, so the editors in [`editing.md`](editing.md) work on it unchanged.

Status: landed for odt, ods, docx and xlsx, in every binding (#950 to #956).
The open items are at the end.

## What there is

- `odr::create_document(FileType)` in `odr.hpp`, and
  `FileTypeCapabilities::create`. A host reads the capability before it
  offers a type in a "New" menu.
- The bindings: `pyodr.create_document`, `Odr.createDocument` in java,
  `Document.create(fileType:)` in swift, `odr.create` in npm.
- The parts: `internal/odf/odf_blank.cpp` and `internal/ooxml/ooxml_blank.cpp`.

## Decisions

### 1. The library generates the package, no file is committed

A file that Word or LibreOffice saves holds rsids, a full theme, latent
styles, font tables and the version of the producer. Nobody can explain most
of it, and a change to it is a binary diff. So each format keeps its minimal
parts as string literals, which a reviewer reads in a diff.

### 2. A new document goes through the read path

`create_document` writes the parts into an in-memory zip and opens the bytes
with `odr::open`. There is no second document model, so every edit operation
and `save` works on a new document, and it cannot act differently from an
opened one. The npm binding opens the saved bytes once more, because its
session holds a decoded file.

### 3. A new document is not empty

Every structural edit has an anchor, so a text document holds one empty
paragraph, and a spreadsheet holds one sheet `Sheet1`. An ods sheet also holds
one column and one row, because the ODF schema requires them in a table.

### 4. The argument is a `FileType`

`DocumentType::text` does not choose between odt and docx.

### 5. The defaults are the defaults of the native producer

| Type | Font | Page |
|------|------|------|
| odt | Liberation Serif 12pt | A4, margins 2 cm |
| ods | Liberation Sans 10pt | A4, margins 2 cm |
| docx | Calibri 11pt, line spacing 1.08, 8pt after | A4, margins 1 inch |
| xlsx | Calibri 11pt | A4, the margins of Excel |

Word 365 uses Aptos. We use Calibri because more systems map it to a
metric-compatible font (Carlito).

### 6. The metadata names us and holds no date

`meta:generator` and `Application` are `odr`. With no date, the bytes of a new
document are the same on every call, and a test can compare them.

## How to check a change to the parts

Our own reader accepts packages that other readers refuse, so `odr_test`
alone proves little.

1. Run `DocumentCreate.*` in `odr_test`.
2. Load each new and each edited file with LibreOffice `--convert-to pdf`.
3. Validate each docx and xlsx with the Open XML SDK `OpenXmlValidator`.
   This checks the parts against the Office schemas.

## Open items

- A check in Word and Excel. Nobody has opened a new docx or xlsx in them
  yet.
- odp, pptx and odg. They need an operation that adds a slide or a page and
  one that adds a text frame. They need their own design.
- A Letter page. The page is A4 for every type. An options struct can add a
  Letter page when a host asks for it.
- "New document" in the droid and ios apps.

Not planned: txt (a new text file is an empty string), flat ODF and templates
(a host opens a template it owns).
