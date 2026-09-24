# AGENTS.md — `internal/csv`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file covers what
csv does differently, and why. Open work is in [`PLAN.md`](PLAN.md).

## Cells are not elements

Csv uses no `ElementRegistry`. A registry costs an entry per cell, and the
renderer asks for at most `spreadsheet_limit` rows of them. So an id is the
coordinate, packed into the 64 bits of `ElementIdentifier`
(`csv_document.cpp`):

```
63..61  kind    root | sheet | cell | text
60..24  row     37 bits
23..0   column  24 bits
```

`null_element_id` is zero, so no kind may be zero. The adapter decodes an id
instead of looking it up.

A sheet's cells are not reachable by walking. `element_first_child` of a sheet
is `null_element_id`. Cells come from `SheetAdapter::sheet_cell(column, row)`,
which is how the renderer asks for them.

## Everything goes through `cell` and `dimensions`

`CsvDocument` holds the whole file decoded. The adapter reaches the data only
through `cell(column, row)` and `dimensions()`. That is the seam a later
streaming implementation needs.

## Detection rejects; the parser does not

- `probe` (`csv_util`) is detection. It scores a bounded sample and may say
  "not a csv". Its rules are heuristics for an unknown file: at least two
  columns, and no dangling quote in a complete file.
- `RecordReader` is parsing. Given a separator it is total: ragged rows, one
  column, an empty file and a truncated quoted field all read as some csv. A
  short row pads, a long one widens.

So a one-column csv is legitimate, and `CsvOptions{.separator = ','}` reads
it. `NoCsvFile` is a detection failure only. An incoherent dialect, such as a
separator equal to the quote, throws `std::invalid_argument`.

Options come in through `DecodeOptions::as_csv`. An unset field is detected.
`CsvFile::options()` returns every field resolved, so a caller can show what
was detected and offer an override.

## Value types are decided per column

`ValueType::float_number` drives one CSS class, right alignment. A column is
numeric only if every value below the header passes `is_number`. The first row
is left out, because a header names its column. The grammar is strict: a
leading zero and a thousands separator both reject, because `007` is an
identifier and `1,234` means two numbers depending on the locale. Dates are
never guessed. Quoting carries no type information.

## A csv holds a text file and also loads as a document

A csv is `FileCategory::text` with `DocumentType::spreadsheet`, so
`is_text_file()` and `is_document_file()` are both false. `abstract::CsvFile`
holds a `text::TextFile`, which `CsvFile::text_file()` hands out.
`CsvFile::document()` is the other view of the same bytes. Opening the file as
`FileType::text_file` is the escape hatch when detection was wrong.

Cell text is UTF-8, because `Text::content()` returns `std::string` and every
binding treats it as UTF-8. An encoding `internal/encoding` cannot decode has
no document, while the text rendering path stays open to it.
