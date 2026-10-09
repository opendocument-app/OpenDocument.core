# JSON plan

## Current behavior

`JsonFile` validates `TextFile::text()` as decoded UTF-8 with
`nlohmann::json::accept`. Validation reads the whole file but builds no value
tree. UTF-16 and UTF-32 sources are decoded before validation. Invalid JSON
falls back to plain text during automatic detection.

JSON remains `FileCategory::text` with `is_decodable() == false` and renders
through `create_text_service`. Tests are in
`test/src/internal/json/json_file_test.cpp`; HTML reference tests skip JSON.

## Bounded detection

- Probe at most 64 KiB through `encoding::read_probe` and `encoding::to_utf8`.
- Use SAX validation without a DOM. Accept a truncated final value only when
  the probe ends before the file does.
- Automatic detection should require `{` or `[` first; explicit JSON opening
  should continue to accept scalars.
- Explicit opening validates the complete file. A future structured renderer
  must validate any file accepted by a partial probe.

## Structured source view

- Add `html/json_file.*`, modeled on the XML source view, with one
  `json.html` view. Keep JSON outside the document element model.
- Parse decoded UTF-8 once in the service and retain its DOM. Keep the
  nlohmann headers within the JSON implementation.
- Render objects and arrays as open `<details>` with child counts in their
  summaries. Distinguish keys, strings, numbers and literals with CSS classes.
- Escape all text through `html::escape_text`. DOM parsing normalizes number
  spelling; the text view preserves it.
- Cap emitted values through an optional `HtmlConfig` budget. Show a visible
  truncation notice and omit the remaining DOM nodes.
- Dispatch from the text overload of `html::translate`, retaining the plain
  text fallback if full parsing fails.
- Add inline fixtures and remove JSON from the HTML reference-test skip list.

## Later options

Configurable folding depth, expand/collapse controls, search, JSON pointers
and NDJSON need separate API decisions.
