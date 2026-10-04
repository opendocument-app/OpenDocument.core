# Document creation design

How the library makes a new document of a file type that it can edit. The
editors are in [`editing.md`](editing.md), and creation reuses them unchanged:
a new document is an ordinary `Document`.

Status: planned. The stages are at the end.

## Problem

A host can open, edit and save a document, but it cannot start one. Today an
app that offers "New document" has to ship its own empty file for each format.

## API

```cpp
// odr.hpp
/// A new document of @p type, with one empty paragraph or one empty sheet.
/// @throws UnsupportedFileType if `capabilities_by_file_type(type).create`
///         is false.
[[nodiscard]] Document create_document(FileType type);
```

`FileTypeCapabilities` gets `create`, next to `edit`, `save` and `encrypt`.
A host reads it before it offers a file type in a "New" menu.

## Decisions

### 1. The library generates the package, no file is committed

A file that Word or LibreOffice saves is not empty. It holds rsids, a full
theme, latent styles, font tables and the version of the producer. Nobody can
explain most of that content, and a change to it is a binary diff.

So each format has its minimal parts as string literals in the source. A
reviewer reads them in a diff, and we know every byte that we write.

### 2. A new document goes through the read path

`create_document` writes the parts into an in-memory zip, and then opens the
bytes with `odr::open`. There is no second constructor and no second document
model. So every edit operation and `save` works on a new document with no
extra code, and a new document cannot act differently from an opened one.

The cost is one deflate and one inflate of a few kilobytes.

### 3. A new document is not empty

Every structural edit has an anchor. `insert_paragraph_after` needs a
paragraph, and `append_text` needs a parent. A spreadsheet edit names a sheet
by its index. So:

- A text document holds one empty paragraph of the default paragraph style.
- A spreadsheet holds one empty sheet. Its name is `Sheet1`, as in Excel and
  in LibreOffice.

### 4. The argument is a `FileType`

`DocumentType::text` does not choose between odt and docx. A host already
knows the extension that the user wants, and
`file_type_by_file_extension` gives the type.

### 5. The defaults are the defaults of the native producer

The user sees the defaults, so they must look like a new document of the
native application:

| Type | Font | Page |
|------|------|------|
| odt | Liberation Serif 12pt | A4, margins 2 cm |
| ods | Liberation Sans 10pt | A4, margins 2 cm |
| docx | Calibri 11pt, line spacing 1.08, 8pt after | A4, margins 2.54 cm |
| xlsx | Calibri 11pt | A4, margins 1.78 cm and 1.91 cm |

Word 365 uses Aptos now. We use Calibri because more systems map it to a
metric-compatible font (Carlito). The page is A4 for every type. A Letter page
is a locale choice, and an options struct can add it when a host asks for it.

### 6. The metadata names us and holds no date

`meta:generator` and `Application` are `odr`. The build carries no version
on main, so the name holds none. There is no creation
date, so the bytes of a new document are the same on every call, and a test
can compare them.

## Out of scope

- odp, pptx and odg. There is no operation that adds a slide, a page or a
  frame, so a blank presentation is a dead end. They come with `insert_slide`.
- txt. A new text file is an empty string, and `TextFile` is not a
  `Document`. A host needs no library for it.
- Flat ODF (`.fodt`, `.fods`). The package form is what the apps write.
- Templates (`.ott`, `.dotx`). A template is a document that the host owns, so
  the host opens it.

## Testing

- `odr_test`: for each type, create, check the element tree, edit, save, open
  the saved file and check the edit.
- The capability test: `create` means `edit` and `save`, and
  `create_document` throws for every type without `create`.
- LibreOffice `--convert-to pdf` loads each new and each saved file. It is
  the oracle that a package is valid, because our own reader accepts packages
  that LibreOffice refuses.
- Word opens each new docx and xlsx once by hand. Word refuses a package with
  a missing relationship or content type that LibreOffice accepts.

## Stages

Each stage is one pull request, stacked on the one before it.

1. This document.
2. `create_document`, the `create` capability and odt.
3. ods.
4. docx.
5. xlsx.
6. The bindings: python, java, objective-c and npm.
