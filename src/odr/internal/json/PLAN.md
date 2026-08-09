# JSON plan

Where the json module is going, and in what order. Written before stage 1; keep
it honest as stages land. Sibling of [`csv/PLAN.md`](../csv/PLAN.md), and it
leans on what stage 1 there already landed in `internal/encoding`.

## Today

`JsonFile` is detection only — 35 lines, `is_decodable() == false`,
`FileCategory::text` — so `html::translate` routes it through
`translate(file.as_text_file())` to `html::create_text_service` and a json
renders exactly like a `.txt`: numbered lines, no folding, nothing the raw bytes
did not already say.

`check_json_file` runs `nlohmann::json::parse` over the **whole** stream and
throws the result away (`// TODO limit check size`, `// TODO check if that even
works`, `json_util.cpp:8`). `open_strategy.cpp:288` constructs a `JsonFile`
purely to *name* the type while listing candidates, so a 200 MB json is fully
parsed just to be called a json, and the dom is destroyed immediately.

The parse reads the **undecoded** stream — `check_json_file(*m_file->file()->
stream())`, `json_file.cpp:10` — even though `JsonFile` holds a
`text::TextFile` whose `text()` has decoded to UTF-8 since the csv plan's stage
1. nlohmann accepts UTF-8 and skips a UTF-8 BOM, so a UTF-16 json — blessed by
RFC 4627, still emitted by Windows tooling — fails detection today and falls
back to plain text, where `html/text_file.cpp` then renders it correctly. The
rendering path is the one that already got fixed; the parse is what is left.

json is on the skip list in `html_output_test.cpp:115`, and there is no
`test/src/internal/json/` at all.

`nlohmann_json` is already a direct link dependency (`CMakeLists.txt:272`).

## Target

A json opens on its value tree: indented, foldable, keys visually distinct from
values, and a collapsed subtree saying how much it hides. Read-only, no
round-trip, no schema. The text view stays reachable and stays the floor.

## Decisions taken up front

**Render the parsed tree, not the bytes.** Colouring the existing text service
with a lexer is what "syntax highlighting" would mean, and it is the wrong axis:
what makes a large json readable is *folding and structure*, not colour, and
anyone who wants the literal bytes already has the text view. We parse the file
anyway.

That said, a little colour is nearly free here and worth taking. Walking a typed
dom, every scalar already knows whether it is a string, a number, a boolean or
null, so four CSS classes plus one for keys cost an `if` in the writer — no
lexer, no grammar, nothing that can disagree with the parser. That is the whole
of the "highlighting" this needs.

**Not a document.** `ElementType` (`document_element.hpp:87`) has no key/value
pair. Mapping objects onto `list`/`list_item` either drops the keys or smuggles
them into `text`, and then every binding walking the tree sees prose where the
file had structure — worse than the honest answer, which is that the element
model does not describe json. So `is_decodable()` stays `false`, json stays
`FileCategory::text`, and the html service is hand-written like
`html/text_file.cpp` and `html/pdf_file.cpp` rather than derived from the
generic renderer. That is a deliberate deviation from step 3 of "Adding /
extending a document format" in the root `AGENTS.md`; it belongs in a
`json/AGENTS.md` once stage 3 lands.

If a json-as-data api is ever wanted, the honest shape is a small value api in
the public headers, not an `ElementAdapter`. Nothing here forecloses it.

**The service parses; the file detects.** The tempting move is to keep the dom
on `JsonFile` so it is parsed once. Resist it. It puts `nlohmann/json.hpp` — the
single heaviest header in the build — into `json_file.hpp` and from there into
`open_strategy.cpp`; the escape (`nlohmann/json_fwd.hpp` plus a
`std::unique_ptr` member and an out-of-line destructor) is a real technique but
it buys nothing, because after stage 2 detection no longer builds a dom to
share. So: detection reads a bounded prefix and keeps nothing, the html service
parses `TextFile::text()` once in its constructor and owns the dom for its own
lifetime, and nlohmann stays confined to `json_util.cpp` and
`html/json_file.cpp`.

**The text view is the floor.** A bounded probe accepts a prefix; the full parse
can still fail on byte 900 000. So `html::translate(const TextFile &)` builds
the json service inside a `try` and falls back to `create_text_service` when the
parse throws — a broken json renders as text with its syntax error visible,
which is the useful outcome. This is the one place the fallback lives; no other
call site needs to know.

**Dispatch by narrowing, not by a new handle.** `translate(const DecodedFile &)`
switches on category (`html.cpp:213`) and json is `text`, so json arrives at
`translate(const TextFile &)`. Branch there on
`file_type() == FileType::javascript_object_notation`. The alternative — a
public `JsonFile` handle with `as_json_file()`, the way csv just got one — costs
the handle plus its surface in four bindings, and csv needed it only because it
had `options()` and `document()` to hang there. json has neither. Revisit if
json ever grows per-file options; view-shaping knobs belong in `HtmlConfig`
anyway.

**Detection is a discriminator, not a validity check.** RFC 8259 allows a bare
scalar at the top level, so `42` and `"hello"` are valid json documents — and
also valid txt, and valid one-column csv. Requiring the first token to be `{` or
`[` is the same kind of rule as csv's "at least two columns": it is about
telling formats apart, not about what json is. The by-type path
(`open_strategy.cpp:167`, reached when the caller says "this is json" or the
extension does) parses in full and accepts what RFC 8259 accepts.

**Number spelling is not preserved.** nlohmann keeps `std::int64_t` /
`std::uint64_t` / `double`, not the source token, so re-serialising a scalar
gives the shortest round-trip form: `1.50` renders as `1.5`, `1e3` as `1000.0`.
The value is faithful, the spelling is not. Accepted — the text view is one
click away and shows the file. Emitting the raw token instead would mean a
second, token-preserving parse, which is a large cost for a cosmetic gain.

**Strings out of the dom are valid UTF-8.** nlohmann rejects invalid UTF-8 at
parse time, so the writer never has to sanitise — it takes
`get_ref<const std::string &>()` straight through `html::escape_text`
(`html/common.hpp:48`). Only the *probe* can hold a `U+FFFD`, from a multi-byte
sequence cut at the boundary, and a replacement character inside a string
literal is syntactically fine.

**Folding is `<details>`/`<summary>`.** Native, scriptless, static html, and it
survives the fragment-writing `HtmlService` shape without a frontend object. The
summary line carries what the fold hides — `"users": [ … 128 items ]` — which
the dom knows for free. Expand-all/collapse-all needs script; add it when
someone asks.

**Truncation is loud.** The renderer emits one row per scalar, so the browser's
node budget is the real limit and it arrives long before ours does. Collapsing
does not help: a `<details>`-hidden subtree is still in the DOM. So cap the
number of *emitted* values and close the output with a visible
`… 4 213 891 more values, not rendered`, rather than silently stopping.
`HtmlConfig::spreadsheet_limit` (`html.hpp:124`) is the precedent for where the
knob goes. Streaming a json too large for that is out of scope, exactly as csv
stage 5 is.

---

## Stage 1 — parse decoded text

A bug fix that ships alone, and the smallest possible diff.

- `check_json_file` takes a `std::string_view` of decoded UTF-8 instead of an
  `std::istream` of raw bytes; `JsonFile` passes `m_file->text()`.
- a non-decodable encoding is not a json: RFC 8259 §8.1 makes json Unicode by
  definition, so there is nothing to fall back to. `text()` already throws
  `UnsupportedTextEncoding`, and `open_strategy`'s `catch (...)` already turns
  that into "not a json".
- first `test/src/internal/json/json_util_test.cpp`, inline string literals: an
  object, an array, a bare scalar, a truncated document, a UTF-16LE json with a
  BOM.

## Stage 2 — bounded detection

Stops type listing paying for a full dom, and splits detection from parsing the
way the csv plan did — same reason, same shape.

- probe with `encoding::read_probe` + `encoding::to_utf8`
  (`encoding/detect.hpp`, `encoding/transcode.hpp`), 64 KiB, the same default
  the rest of the codebase uses.
- verdict from `nlohmann::json::sax_parse` with a callback that keeps no dom and
  stops at the cut: first non-whitespace byte is `{` or `[`, and no syntax error
  before the end of the probe. A prefix is *supposed* to end mid-document, so
  running out of input is not a rejection — only a real syntax error is.
- `open_strategy` keeps constructing a `JsonFile` to probe, because that
  constructor *is* the probe now and is bounded — as with csv.
- the by-type path stays a full parse and keeps accepting top-level scalars.

## Stage 3 — the tree view

- `internal/html/json_file.{hpp,cpp}` with `create_json_service`, alongside
  `create_text_service`; one view, `json.html`.
- `html::translate(const TextFile &)` narrows on file type and falls back to the
  text service when the full parse throws.
- the writer: nested `<div>`s with a CSS indent (not `<pre>` — we control every
  byte of the output), `<details>`/`<summary>` per object and array with the
  child count in the summary, `odr-json-key` / `-string` / `-number` /
  `-literal` classes, `escape_text` on everything, node budget with the loud
  marker.
- `write_json_style` in `html/frontend.cpp` next to `write_text_style`.
- drop `javascript_object_notation` from the skip list at
  `html_output_test.cpp:115`.

## Stage 4 — only if wanted

Auto-expand depth and node budget as `HtmlConfig` fields; expand/collapse-all
and search (both need script); copy-a-json-pointer per row; NDJSON
(`application/x-ndjson`) as a separate file type rendering as a list of trees.
None of it is needed for the view to be worth having.
