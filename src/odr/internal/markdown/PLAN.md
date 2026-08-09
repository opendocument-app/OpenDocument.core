# Markdown plan

Where markdown support is going, and in what order. Written before stage 1;
keep it honest as stages land.

## Today

`FileType::markdown` exists (`file.hpp:73`) and has a table row with extensions
and mime types (`file_type_table.cpp:431`), declared classification-only:
capabilities `{}`, `FileCategory::text`, `DocumentType::unknown`. Nothing
constructs it. `magic` does not know it, `open_strategy` never names it, and
`open_file(file, FileType::markdown)` falls through to
`UnsupportedFileType` (`open_strategy.cpp:198`). A `.md` opened today comes back
as `text_file` and renders as a line list through
`html::create_text_service`.

So there is no code to undo. The work is a decoder, plus flipping the row.

## Target

A markdown file opens as a **text document** — `TextRoot` with paragraphs,
spans, lists, tables, links and images — so the generic HTML renderer and every
binding get it without format-specific code. That is the whole argument for
doing it this way rather than writing a markdown→HTML renderer next to
`html/text_file.cpp`: the latter would produce html output only, with no
element api, nothing for JNI/embind/pybind/ObjC, and no path to
`back_translate`.

CommonMark is the base; GFM's tables, strikethrough, task lists and permissive
autolinks are the extensions worth having. Everything beyond that is somebody's
dialect and stays out.

## Decisions taken up front

**A library, not a hand-rolled parser.** This is the opposite call from
`d8c8715` (dropping vincentlaucsb-csv-parser to scan csv in-tree), and
deliberately so. Csv is a hundred lines of quote-aware scanning. CommonMark has
~650 conformance cases, and the places hand-written parsers rot — lazy
continuation, link reference definitions, the emphasis left/right-flanking
rules, list-item indent arithmetic — are exactly the ones that look easy for a
weekend and are then wrong forever.

**md4c** (`md4c/0.5.2` on conancenter, MIT, ~5k LOC C). Chosen over cmark-gfm:
cmark-gfm has no conan recipe at all, builds an allocating AST we would
immediately walk once and throw away, and is a GitHub fork with a lax release
cadence. md4c is a SAX parser — `enter_block` / `leave_block` / `enter_span` /
`leave_span` / `text` — which maps onto `create_element` / `append_child` with a
single id stack and no intermediate tree. Its `MD_DIALECT_GITHUB` flag is
exactly the extension set above. Add it to `conanfile.py:51` next to
`nlohmann_json` / `uchardet`, and `find_package` + link in `CMakeLists.txt:59`
/ `:271`.

Two things to check when wiring it up, because they decide small amounts of
work in stage 2:
- whether the conan package exports the `md4c-html` component, whose
  `entity.h`/`entity_lookup` resolves the ~2000 named html entities. If not, we
  handle `&amp;`-class and numeric references ourselves and pass the rest
  through literally.
- md4c parses bytes and assumes UTF-8 (`MD4C_USE_UTF8`), so decoding happens
  before it, not inside it.

**Markdown is a `DocumentFile`, not a `TextFile`.** `abstract::TextFile` fixes
`file_category()` to `text` (`abstract/file.hpp`); a document has to be
`FileCategory::document` with `DocumentType::text`. The table row changes
category with it. This is api-visible for `FileType::markdown` — and free,
because the row declares no capabilities today, so nothing can be relying on
it.

**Input is UTF-8, produced by `internal/encoding`.** `MarkdownFile` takes a
`std::shared_ptr<text::TextFile>` exactly as `CsvFile` and `JsonFile` do
(`csv_file.hpp`), and calls `text()` for the decoded bytes. An encoding we
cannot decode throws `UnsupportedTextEncoding` (`exceptions.hpp:42`) — the same
rule the csv plan sets for the sheet path, and for the same reason:
`Text::content()` is UTF-8 to every binding, so passing legacy bytes through
and letting a browser sort it out is not available to us.

**Detection is by caller, not by content.** `open_strategy` is entirely
content-driven — magic plus speculative probes — and has no extension path
anywhere. Markdown has no signature, and a content probe for it is a probe for
"prose with occasional punctuation", which is every plain text file with a `#`
comment or an `*` bullet in it. Sniffing would steal `text_file` matches and be
confidently wrong. So: `detect_by_content` stays **false**, markdown never joins
the speculative chain in `list_file_types` (`open_strategy.cpp:272`), and the
only way in is `DecodedFile(file, FileType::markdown)` (`file.hpp:341`) via a
new branch in `open_file` next to the text/csv/json ones
(`open_strategy.cpp:146`). Callers route on the filename, which is what they
already have.

The consequence to accept: `.md` still opens as `text_file` by default. That is
correct behaviour for a format that is, by construction, valid plain text.

**There is no `NoMarkdownFile`.** Every other format's exception exists because
detection rejects. Nothing rejects here: md4c is total — any UTF-8 byte
sequence is some markdown document — and there is no detection to fail. The
only failure mode is the undecodable encoding above.

**Headings ride on paragraphs, as ODF already does.** `ElementType` has no
heading (`document_element.hpp:87`), and `odf_parser.cpp:304` maps `text:h` to
`ElementType::paragraph` with the level carried in the style. Markdown does the
same: `MD_BLOCK_H` with `detail->level` becomes a paragraph plus a `TextStyle`
holding `font_size` and `font_weight` from a small `StyleRegistry`. The cost is
honest and shared with odt — the html output is `<p>` with inline styles, not
`<h1>`, so heading semantics are lost to screen readers and to anything walking
the api for an outline. Stage 5 is where that gets fixed properly, once, for
both engines.

**Raw html is dropped.** CommonMark passes html blocks and inline html through
verbatim. There is no passthrough element in the model, and inventing one means
deciding what `Text::content()` returns for it in four bindings — a real
question, not a markdown question. `MD_BLOCK_HTML` and `MD_SPAN`-level raw html
are skipped in stage 1. Inline `<svg>` in a markdown file therefore renders as
nothing, which is worse than the status quo for exactly one use case, and is
the price of not opening that seam early.

**Images stay external.** `ImageAdapter` (`abstract/document.hpp:495`) offers
`image_is_internal()` plus `image_href()`, so `![](diagram.svg)` becomes a
`Frame` + `Image` with `is_internal() == false` and the href passed through
untouched. No filesystem, no relative-path resolution, no fetching. A viewer
resolves it against wherever it got the document, which is the only party that
knows.

## Graphs and drawings

Worth separating two things that get said in one breath.

*Inline SVG* is the raw-html question above, and blocked behind the same
decision.

*Mermaid / graphviz / plantuml fences* are not a markdown feature at all — they
are a fenced code block with an info string, which a downstream renderer picks
up. Rendering them in C++ means embedding a graph layout engine, which is a
larger project than markdown support entire, and one that duplicates what every
frontend already ships. The right move is to keep the fence as a code block
that *carries its language*, and let the frontend render it. There is nowhere
to put that language today — which, with the heading level, makes two entries
for the same stage 5.

## Element mapping

| md4c | model |
|---|---|
| `MD_BLOCK_DOC` | `root` (`TextRoot`), default `PageLayout` as `doc_document.cpp:144` |
| `MD_BLOCK_H` (level 1–6) | `paragraph` + heading `TextStyle` |
| `MD_BLOCK_P` | `paragraph` |
| `MD_BLOCK_UL` / `OL` | `list` |
| `MD_BLOCK_LI` | `list_item` |
| `MD_BLOCK_QUOTE` | `group` + left `margin` on the paragraphs inside |
| `MD_BLOCK_CODE` | `paragraph` + monospace `TextStyle`; info string dropped |
| `MD_BLOCK_HR` | — (nothing in the model) |
| `MD_BLOCK_TABLE` / `THEAD` / `TBODY` / `TR` / `TH` / `TD` | `table` / `table_row` / `table_cell` |
| `MD_SPAN_EM` / `STRONG` / `DEL` | `span` + `font_style` / `font_weight` / `font_line_through` |
| `MD_SPAN_CODE` | `span` + monospace `font_name` |
| `MD_SPAN_A` | `link` |
| `MD_SPAN_IMG` | `frame` + `image` |
| `MD_TEXT_NORMAL` / `ENTITY` / `CODE` | `text` |
| `MD_TEXT_BR` | `line_break` |
| `MD_TEXT_SOFTBR` | a space appended to the current text |
| `MD_TEXT_NULLCHAR` | U+FFFD |

Two renderer gaps this exposes, both pre-existing:
`html::translate_list` hardcodes `<ul>` (`html/document_element.cpp:342`), so an
ordered list renders as bullets; and `ElementType::group` renders as its
children with no wrapper (`:72`), so a blockquote's structure survives only in
the margins its paragraphs carry.

## Module layout

Mirrors `oldms/text`, which is the reference the root `AGENTS.md` points at.

```
src/odr/internal/markdown/
  markdown_file.hpp/.cpp              abstract::DocumentFile over a text::TextFile
  markdown_document.hpp/.cpp          internal::Document + the ElementAdapter
  markdown_element_registry.hpp/.cpp  flat vector, side maps for text/link/image
  markdown_parser.hpp/.cpp            md4c callbacks → registry, one id stack
  markdown_style.hpp/.cpp             StyleRegistry: heading sizes, code font, quote margin
```

Every `.cpp` goes into `ODR_SOURCE_FILES` (`CMakeLists.txt:86`).

---

## Stage 1 — blocks

The skeleton, end to end, with the least that is worth rendering.

- md4c as a conan dependency; `MarkdownFile` over `text::TextFile`; the
  `open_file` branch; the table row flipped to `FileCategory::document`,
  `DocumentType::text`, `{.open = true, .translate_html = true}`.
- registry, adapter and `Document` per the pattern; `TextRootAdapter`,
  `ParagraphAdapter`, `TextAdapter`, `LineBreakAdapter`.
- the block half of the table above: doc, headings, paragraphs, code blocks.
  Lists and quotes land here too — they are blocks and cost a stack push each.
- tests inline, string literal in, element tree out; no fixture files
  (`text_file_test.cpp` is the shape).

`FileTypeCapabilities.declaration_matches_the_engines`
(`odr_test.cpp:163`) opens two files per type from the test data and asserts the
engines do not exceed the row, so a handful of `.md` samples go into the
test-data repo alongside this stage.

## Stage 2 — inlines and styles

- `SpanAdapter`, `LinkAdapter`; emphasis, strong, inline code, links.
- `markdown_style`: one `StyleRegistry` handing out the heading scale, the
  monospace face and the quote margin, indexed from the registry the way
  `doc_style` is.
- entity and soft-break handling per the table; the `entity_lookup` question
  above resolves here.

## Stage 3 — GFM

`MD_DIALECT_GITHUB`: tables, strikethrough, task lists, permissive autolinks.
Tables are the substantial one — `TableAdapter` plus row/column/cell — and the
reason to do it before images: it is what people actually put in readmes.

Task list items have no checkbox in the model. Render the box as text (`☐`/`☑`)
in the item's first text element rather than inventing an element type for it.

## Stage 4 — images and frontmatter

- `FrameAdapter` + `ImageAdapter`, external hrefs per the decision above.
- YAML/TOML frontmatter: md4c does not know it, so strip a leading `---` fence
  before parsing and expose what it holds through `FileMeta`. Parse only the
  flat scalars we have somewhere to put (`title`, `author`, `date`); do not
  take on a YAML dependency for the rest.

## Stage 5 — the two model gaps

Both are api changes shared with the other engines, which is why they come
last and together rather than being smuggled in with the parser.

- **heading level** — the odf mapping loses it too. Either an
  `ElementType::heading` with a level, or a level on `ParagraphStyle`. The
  latter is smaller and lets `html::translate_paragraph` emit `<h1>`…`<h6>` for
  odt and markdown at once; the former is more honest about it being a
  different kind of thing. Decide with odf in the room.
- **code-block language** — a `std::string` on the paragraph, or a dedicated
  code element. This is what makes the mermaid story work without odr rendering
  anything, and it is the only reason a frontend can syntax-highlight.

Deferred beyond this: raw html passthrough, footnotes, definition lists,
`back_translate` to markdown, and any dialect that is not GFM.
