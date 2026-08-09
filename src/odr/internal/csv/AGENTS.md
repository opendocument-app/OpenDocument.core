# AGENTS.md — `internal/csv`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file covers what
csv does differently, and why.

## Cells are not elements

The root `AGENTS.md` prescribes an `ElementRegistry`: a flat `std::vector` of
elements, id = index + 1. **Csv does not use one**, deliberately.

A registry costs an entry per element. A sheet has one per *cell*, so a file
with a million rows would cost millions of entries before a single one is
looked at — and `spreadsheet_limit` means the renderer will ask for ten
thousand rows of them at most.

`ElementIdentifier` is a `std::uint64_t`, which is room to spare:

```
63..61  kind    root | sheet | cell | text
60..24  row     37 bits
23..0   column  24 bits
```

So an id *is* the coordinate, and the adapter decodes rather than looks up.
`null_element_id` is zero, so no kind may be.

The consequence to respect: **a sheet's cells are not reachable by walking**.
`element_first_child` of a sheet is `null_element_id`; cells come from
`SheetAdapter::sheet_cell(column, row)`, which is how the renderer asks for
them anyway (`html/document_element.cpp:163`).

## Everything goes through `cell` and `dimensions`

`CsvDocument` holds the whole file decoded, and the adapter never touches that
storage — it calls `cell(column, row)` and `dimensions()`. That is the seam a
later streaming implementation needs: an index and a window can move in behind
those two without the adapter noticing.

## Detection rejects; the parser does not

Two jobs, two places, and mixing them is the mistake this module already made
once.

- `probe` is detection. It scores a bounded sample and may say "not a csv".
  Its rules — at least two columns, no dangling quote in a complete file — are
  *heuristics for recognising an unknown file*, not statements about validity.
- `RecordReader` is parsing. Given a separator it is total: ragged rows, one
  column, an empty file and a truncated quoted field all read as some csv.

So a one-column csv is perfectly legitimate and `CsvOptions{.separator = ','}`
reads it. `NoCsvFile` is a detection failure only. An incoherent dialect — a
separator equal to the quote, a line break as a separator — is
`std::invalid_argument`, a caller mistake rather than bad input.

## A csv is a text file that also loads as a document

`FileCategory::text`, `DocumentType::spreadsheet` — so `is_text_file()` is true
for a csv and `is_document_file()` is false. `abstract::CsvFile` derives from
`abstract::TextFile`, and `CsvFile::document()` is the *other* view of the same
bytes rather than the only one: `TextFile::text()` keeps working, so reading a
csv as text needs no reopening. Opening it as `FileType::text_file` stays the
escape hatch when detection was wrong about it being a csv at all.

The one thing that costs: `html::translate` has to test `is_csv_file()` ahead of
its text branch (`html.cpp:215`), because a csv answers `is_text_file()` and a
line list is never what a viewer wants from a table.

Text has to be UTF-8 by the time it reaches a cell: `Text::content()` returns
`std::string` and every binding treats it as UTF-8. That is why an encoding
`internal/encoding` cannot decode has no document at all, while the *text*
rendering path stays open to it.
