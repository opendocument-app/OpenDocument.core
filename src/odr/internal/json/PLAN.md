# JSON plan

Nothing of this has landed. Sibling of [`csv/PLAN.md`](../csv/PLAN.md); it
uses what `internal/encoding` already provides.

## Today

`JsonFile` is detection only: `is_decodable() == false`,
`FileCategory::text`. `html::translate` routes it to `create_text_service`, so
a json renders like a `.txt`.

`check_json_file(std::istream &)` runs `nlohmann::json::parse` over the whole
undecoded stream and throws the result away (`json_util.cpp`). `open_strategy`
constructs a `JsonFile` only to name the type, so a 200 MB json is fully
parsed to be called a json. Because the parse reads raw bytes, a UTF-16 json
fails detection and falls back to plain text, where `html/text_file.cpp`
renders it correctly.

json is on the skip list in `html_output_test.cpp`, and there is no
`test/src/internal/json/`. `nlohmann_json` is a direct link dependency.

## Target

A json opens on its value tree: indented, foldable, keys distinct from values,
and a collapsed subtree that says how much it hides. Read-only. The text view
stays reachable.

## Decisions

- **Render the parsed tree, not the bytes.** Folding and structure make a
  large json readable, not colour. A typed dom gives four scalar classes plus
  one for keys with no lexer.
- **Not a document.** `ElementType` has no key/value pair, and a mapping onto
  `list` drops the keys. So `is_decodable()` stays false, json stays
  `FileCategory::text`, and the html service is hand-written like
  `html/text_file.cpp`. This deviates from step 3 of "Adding a document
  format" in the root `AGENTS.md`. If a json data API is ever wanted, it is a
  small value API in the public headers, not an `ElementAdapter`.
- **The service parses; the file detects.** Keep the dom off `JsonFile`, so
  `nlohmann/json.hpp` stays out of `json_file.hpp` and `open_strategy.cpp`.
  Detection reads a bounded prefix and keeps nothing. The service parses
  `TextFile::text()` once in its constructor and owns the dom. nlohmann stays
  confined to `json_util.cpp` and `html/json_file.cpp`.
- **The text view is the floor.** A bounded probe accepts a prefix, and the
  full parse can still fail later. `html::translate(const TextFile &)` builds
  the json service in a `try` and falls back to `create_text_service` when the
  parse throws.
- **Dispatch by narrowing, not by a new handle.** `translate(const
  DecodedFile &)` switches on category, and json is text. Branch on
  `file_type() == FileType::javascript_object_notation` in the text overload.
  csv needed a handle for `options()` and `document()`; json has neither.
- **Detection is a discriminator.** RFC 8259 allows a bare scalar at the top
  level, but `42` is also a valid txt. Detection requires `{` or `[` first.
  The by-type path parses in full and accepts what RFC 8259 accepts.
- **Number spelling is not preserved.** nlohmann keeps the value, not the
  token: `1.50` renders as `1.5`. The text view shows the file.
- **Strings out of the dom are valid UTF-8.** nlohmann rejects invalid UTF-8,
  so the writer passes strings straight through `html::escape_text`.
- **Folding is `<details>`/`<summary>`.** Native, scriptless, static html. The
  summary says what the fold hides: `"users": [ … 128 items ]`.
- **Truncation is loud.** The browser's node budget is the real limit, and a
  hidden subtree is still in the DOM. Cap the emitted values and close with a
  visible `… N more values, not rendered`. `HtmlConfig::spreadsheet_limit` is
  the precedent for the knob.

## Stage 1 — parse decoded text

- `check_json_file` takes a `std::string_view` of decoded UTF-8; `JsonFile`
  passes `m_file->text()`.
- A non-decodable encoding is not a json (RFC 8259 §8.1). `text()` throws
  `UnsupportedTextEncoding`, and `open_strategy` already turns that into "not
  a json".
- First `test/src/internal/json/json_util_test.cpp`, inline string literals:
  an object, an array, a bare scalar, a truncated document, a UTF-16LE json
  with a BOM.

## Stage 2 — bounded detection

- Probe with `encoding::read_probe` and `encoding::to_utf8`, 64 KiB.
- Verdict from `nlohmann::json::sax_parse` with a callback that keeps no dom:
  first non-whitespace byte is `{` or `[`, and no syntax error before the end
  of the probe. Running out of input is not a rejection.
- `open_strategy` keeps constructing a `JsonFile` to probe, because the
  constructor is the probe and is bounded.

## Stage 3 — the tree view

- `internal/html/json_file.{hpp,cpp}` with `create_json_service`; one view,
  `json.html`.
- `html::translate(const TextFile &)` narrows on file type and falls back to
  the text service when the parse throws.
- The writer: nested `<div>`s with a CSS indent, `<details>`/`<summary>` per
  object and array with the child count, `odr-json-key` / `-string` /
  `-number` / `-literal` classes, `escape_text` on everything, the node budget
  with the loud marker.
- `write_json_style` in `html/frontend.cpp` next to `write_text_style`.
- Drop `javascript_object_notation` from the skip list in
  `html_output_test.cpp`.

## Stage 4 — only if wanted

Auto-expand depth and node budget as `HtmlConfig` fields, expand-all and
collapse-all and search (script), a json pointer per row, NDJSON as a separate
file type.
