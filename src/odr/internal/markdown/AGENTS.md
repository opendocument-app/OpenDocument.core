# AGENTS.md — `internal/markdown`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file holds what
markdown does differently, and why. [`PLAN.md`](PLAN.md) holds the open work.

## A text file that also loads as a document

The row is `FileCategory::text` with `DocumentType::text`, the shape
`abstract::CsvFile` has. `abstract::MarkdownFile` derives from
`abstract::DecodedFile` and holds a `text::TextFile`, so `is_text_file()` is
false for a `.md`. `as_markdown_file().text_file().text()` is the source and
`as_markdown_file().document()` the prose. `html::translate` takes the
document view, as it does for a csv.

It is composition, not inheritance, because a `TextFile` is what the library
edits and writes back as plain text, and markdown is not that. It is a decoder
to a `TextRoot`, not a markdown-to-HTML renderer, because the element api is
what the bindings get.

## The name detects it, not the content

`detect_by_content` is false. Markdown has no signature, and a content probe
for it matches every plain text file with a `#` or a `*` in it.
`open_strategy::file_type_by_name` reads the extension off `File::name` and
offers markdown once the bytes have decoded as text, ahead of the csv, json
and xml probes. A name only adds a candidate, so a `.md` that holds a zip is
still a zip. Bytes handed to `File::from_memory` without a name need
`DecodedFile(file, FileType::markdown)`.

`NoMarkdownFile` exists only for the `as_markdown_file()` cast. Nothing
rejects at parse time, because any UTF-8 byte sequence is some markdown
document. The one failure is an encoding `internal/encoding` cannot decode,
which throws `UnsupportedTextEncoding`, because `Text::content()` is UTF-8 to
every binding.

## md4c, not a hand-rolled parser

CommonMark has about 650 conformance cases, and lazy continuation, link
reference definitions, emphasis flanking and list-item indent arithmetic are
where a hand-written parser goes wrong. md4c (`md4c/0.5.2` from conan) is a
SAX parser: `enter_block`, `leave_block`, `enter_span`, `leave_span`, `text`.
That maps onto `create_element` and `append_child` with one id stack and no
intermediate tree. Flags: `MD_DIALECT_GITHUB | MD_FLAG_COLLAPSEWHITESPACE`.

The C boundary imposes two rules:

- An exception must not unwind through md4c's frames. Every callback runs
  through `invoke`, which parks the exception in the `Parser` and returns
  non-zero. `parse_tree` rethrows once `md_parse` has returned.
- md4c parses bytes and assumes UTF-8, so `text::TextFile::text()` decodes
  before it.

## Element mapping

| md4c | model |
|---|---|
| `MD_BLOCK_DOC` | `root` with a `PageLayout` that has no size, because markdown is flow content, and a `2em` margin, so that a paged view keeps the text off the edge. |
| `MD_BLOCK_H` | `paragraph` with the heading `TextStyle`, plus a bold `span` |
| `MD_BLOCK_P` | `paragraph` |
| `MD_BLOCK_UL` / `OL` | `list`; `MD_BLOCK_LI` is a `list_item` with its marker |
| `MD_BLOCK_QUOTE` | `group` plus a left `margin` on the paragraphs inside, one step per level |
| `MD_BLOCK_CODE` | `group` of one monospace `paragraph` per line |
| `MD_BLOCK_HR` | nothing |
| `MD_BLOCK_HTML` | dropped |
| `MD_BLOCK_TABLE` / `TR` / `TH` / `TD` | `table` / `table_row` / `table_cell`; `THEAD` and `TBODY` are transparent |
| `MD_SPAN_EM` / `STRONG` / `DEL` / `CODE` | `span` with the one style it means |
| `MD_SPAN_A` | `link` |
| `MD_SPAN_IMG` | transparent; the alt text flows through |
| `MD_TEXT_BR` | `line_break`; `SOFTBR` is a space |
| `MD_TEXT_HTML` | dropped |
| `MD_TEXT_NULLCHAR` | U+FFFD, on the route the enclosing block's text takes |

### Why a heading also gets a span

`html::translate_paragraph` takes only font family and size from a paragraph's
text style (`translate_block_font_style`). Weight, slant and decoration come
from the spans inside. So a heading is a paragraph with the whole heading
style, where the level survives for the element api, plus a span with
`strong_style()` only. The size stays off the span, because it is in `em` and
would compound. A `TH` cell is built the same way.

### Why a code block is one paragraph per line

The model has no pre-formatted block and `ParagraphStyle` has no
`white-space`, so one paragraph would collapse the newlines. `html::escape_text`
turns leading and doubled spaces into `&nbsp;`, so indentation survives. The
info string is dropped. There is nowhere to put a language yet (`PLAN.md`).

### Why a tight list item opens a paragraph of its own

md4c omits `MD_BLOCK_P` inside a tight list item, and
`html::translate_list_item` writes the marker into the item's first paragraph.
Without one the marker is lost. `open_implicit_paragraph_` opens one when
inline content lands in a `list_item`. The next block or the item's `leave`
closes it.

### Why the columns hang off their own chain

A table's children are its rows. `Table` keeps `first_column_id` and
`last_column_id`, and `append_column` links the columns separately, as `odf`
does. A column on the row chain is not a compile error: the renderer writes a
`<col>` for every row it meets.

### Why a NUL byte follows the block it is in

md4c reports a NUL as `MD_TEXT_NULLCHAR` from a verbatim block too, and then
re-sends the byte at the head of the next `MD_TEXT_CODE` or `MD_TEXT_HTML`
chunk. So the U+FFFD is buffered into `m_code` like the rest of a code block,
dropped like the rest of a raw html block, and the re-sent byte is skipped
where the code text is buffered.

### Nesting is bounded

One block quote opens per `>`, and md4c caps nesting nowhere.
`html::translate_element` recurses with no depth guard, so a deep tree
overflows the stack at render time. `Parser::push_` refuses past `max_depth`
(1024), the same order as `rtf::State::max_depth`.

### Task lists

The model has no checkbox, so the box (`☐` / `☑`) replaces the item's marker.
An ordered item keeps the `number()` the element api reports.

## Known gaps

- Named entities beyond the five XML ones plus `&nbsp;` stay literal. Numeric
  references resolve. md4c matches anything shaped like `&name;`, and the
  conan package ships no `entity.h`. `entity_lookup` is linkable from the
  `md4c-html` component, but its struct is private, and a declared copy of
  that layout corrupts silently on a mismatch. Vendoring the table is the
  other option. Decide before promising CommonMark conformance.
- Raw html is dropped, block and inline. There is no passthrough element, and
  one means deciding what `Text::content()` returns for it in four bindings.
  Inline `<svg>` renders as nothing.
- Horizontal rules, images and frontmatter are not modelled (`PLAN.md`).
