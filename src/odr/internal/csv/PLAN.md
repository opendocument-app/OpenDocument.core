# CSV plan

Open work only. What is built is in [`AGENTS.md`](AGENTS.md).

## Multibyte legacy encodings

`internal/encoding` decodes UTF-8/16/32 and the single-byte legacy encodings.
Shift-JIS, GBK, Big5, EUC-KR, EUC-JP, GB18030 and ISO-2022 are named but not
decodable (`text_encoding_table.cpp`), so a csv in one of them has no document
and renders as text only. Two routes exist, and neither is chosen until
someone needs CJK csv:

- Generate the tables from Python's codecs. Exact, about 85 000 pairs, about
  340 KB of static data as sorted `(uint16, char16)`, less with the run
  encoding `pdf_cid_data` uses.
- Share the RKSJ CMaps `pdf_cid_data.cpp` already compiles in. No size cost,
  but the extraction into `internal/encoding` is work, and a character
  collection's UCS2 map is lossy.

## Large files

Deferred. The renderer caps at `spreadsheet_limit`, so streaming buys nothing
for html output. It matters for open cost and for API consumers that walk the
sheet. `CsvDocument::cell` and `dimensions` are the seam.

- Row-start offsets are the only index needed, because a row boundary is the
  one place where "outside quotes" is unambiguous.
- Checkpoint about every 1024 rows, binary search, then scan forward. A full
  offset table for 10 M rows is 80 MB, checkpoints are about 80 KB per GB.
- `sheet_dimensions()` needs an exact count, so it needs one full quote-aware
  scan. Make it lazy, on the first dimension query, and build the checkpoints
  in that pass.
- Unescape into the staged window. Unescaping only shrinks, so each field
  compacts within its own slot and a `string_view` into the window is valid.
  The window also holds the transcoded UTF-8. Keep 2 to 4 windows in an LRU.
- `abstract::File` offers `memory_data()` and `disk_path()`. A csv inside a
  zip has neither, so that path stays fully in memory by decision.
