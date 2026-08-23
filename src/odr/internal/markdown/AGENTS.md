# AGENTS.md — `internal/markdown`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first, and
[`PLAN.md`](PLAN.md) for where this is going. This file covers what markdown
does differently, and why.

## A document, not a text file

`FileCategory::document` / `DocumentType::text`: a markdown file decodes to a
`TextRoot`, so the generic HTML renderer and every binding get it without
format-specific code. That is the whole argument for a decoder rather than a
markdown→HTML renderer next to `html/text_file.cpp` — the latter would produce
html only, with no element api and nothing for JNI/embind/pybind/ObjC.

Unlike csv, markdown is *not* an `abstract::TextFile` that also loads as a
document. `abstract::TextFile` fixes `file_category()` to `text`, and there is
no reason to read markdown as a line list once it parses as prose.

## Nothing detects it, and nothing rejects it

`detect_by_content` is **false**. Markdown has no signature, and a content probe
for it is a probe for "prose with occasional punctuation" — every plain text
file with a `#` comment or an `*` bullet in it. Sniffing would steal `text_file`
matches and be confidently wrong. The only way in is
`DecodedFile(file, FileType::markdown)`; callers route on the file name, which
is what they already have. **A `.md` still opens as `text_file` by default**,
and that is correct for a format that is by construction valid plain text.

There is no `NoMarkdownFile` either. Every other format's exception exists
because detection rejects; nothing rejects here — md4c is total, any UTF-8 byte
sequence is some markdown document. The only failure is an encoding
`internal/encoding` cannot decode, which throws `UnsupportedTextEncoding`:
`Text::content()` is UTF-8 to every binding, so legacy bytes have no document.

## md4c, not a hand-rolled parser

The opposite call from `d8c8715` (dropping a csv library to scan csv in-tree),
and deliberately so. Csv is a hundred lines of quote-aware scanning. CommonMark
has ~650 conformance cases, and the places hand-written parsers rot — lazy
continuation, link reference definitions, emphasis flanking rules, list-item
indent arithmetic — are exactly the ones that look easy for a weekend and are
then wrong forever.

md4c is a SAX parser (`enter_block` / `leave_block` / `enter_span` /
`leave_span` / `text`), which maps onto `create_element` / `append_child` with
one id stack and no intermediate tree. Dialect: `MD_DIALECT_GITHUB` plus
`MD_FLAG_COLLAPSEWHITESPACE`.

Two things the C boundary imposes:

- **An exception must not unwind through md4c's frames.** Every callback runs
  through `invoke`, which parks what it throws in the `Parser` and returns
  non-zero; `parse_tree` rethrows once `md_parse` has returned.
- **md4c parses bytes and assumes UTF-8**, so decoding happens before it, in
  `text::TextFile::text()`, not inside it.

## Element mapping

| md4c | model |
|---|---|
| `MD_BLOCK_DOC` | `root`, default (empty) `PageLayout` — markdown is flow content, not paged |
| `MD_BLOCK_H` | `paragraph` + heading `TextStyle`, plus a bold `span` (see below) |
| `MD_BLOCK_P` | `paragraph` |
| `MD_BLOCK_UL` / `OL` | `list`; `MD_BLOCK_LI` → `list_item` carrying its marker |
| `MD_BLOCK_QUOTE` | `group` + a left `margin` on the paragraphs inside, one step per level |
| `MD_BLOCK_CODE` | `group` of one monospace `paragraph` **per line** |
| `MD_BLOCK_HR` | — (nothing in the model) |
| `MD_BLOCK_HTML` | — (dropped) |
| `MD_BLOCK_TABLE` / `TR` / `TH` / `TD` | `table` / `table_row` / `table_cell`; `THEAD`/`TBODY` are transparent |
| `MD_SPAN_EM` / `STRONG` / `DEL` / `CODE` | `span` + the one style it means |
| `MD_SPAN_A` | `link` |
| `MD_SPAN_IMG` | — (transparent; the alt text flows through as text) |
| `MD_TEXT_BR` | `line_break`; `SOFTBR` is a space |
| `MD_TEXT_NULLCHAR` | U+FFFD |

### Why a heading also gets a span

`html::translate_paragraph` takes only **font family and size** from a
paragraph's text style (`translate_block_font_style`) — weight, slant and
decoration are expected on the spans inside. So a heading is a paragraph
carrying the whole heading style (that is where the level survives for the
element api) plus a span carrying `strong_style()` and nothing else. Only the
weight: the paragraph's size is already in `em`, and repeating it on the span
would compound. A `TH` cell is built the same way.

### Why a code block is one paragraph per line

The model has no pre-formatted block and `ParagraphStyle` has no
`white-space`, so a single paragraph would collapse the newlines. One paragraph
per line survives; `html::escape_text` turns leading and doubled spaces into
`&nbsp;`, so indentation survives with it. The info string (` ```cpp `) is
dropped — there is nowhere to put a language yet, which is stage 5 in
[`PLAN.md`](PLAN.md).

### Why a tight list item opens a paragraph of its own

md4c omits `MD_BLOCK_P` inside a *tight* list item, so its text arrives with no
block around it — and `html::translate_list_item` writes the item's marker
*into its first paragraph*, so without one the marker is silently dropped.
`open_implicit_paragraph_` opens one when inline content lands directly in a
`list_item`; the next block (or the item's `leave`) closes it again.

### Why the columns hang off their own chain

A table's children are its rows. One sibling chain cannot also carry the
columns, so `Table` keeps `first_column_id`/`last_column_id` and
`append_column` links them separately — the same shape `odf` uses. Getting
this wrong is not a compile error: the renderer walks `table_first_column`'s
siblings and happily writes a `<col>` for every row it runs into.

### Task lists

The model has no checkbox, so the box (`☐` / `☑`) *replaces* the item's
marker rather than being invented as an element type.

## Known gaps

- **Named entities beyond the XML five (plus `&nbsp;`) stay literal.** md4c
  matches anything shaped like `&name;` without knowing the list, and the conan
  package ships no `entity.h`. `entity_lookup` *is* linkable from the
  `md4c-html` component, but its struct is private — using it means declaring
  that layout ourselves, where a mismatch corrupts silently rather than failing
  to link. Decide that before promising CommonMark conformance; vendoring the
  table is the other option.
- **Raw html is dropped**, block and inline. There is no passthrough element,
  and inventing one means deciding what `Text::content()` returns for it in four
  bindings — a real question, not a markdown question. Inline `<svg>` therefore
  renders as nothing.
- **A hyperlink's href is passed through** and only `escape_attribute`d by the
  renderer, exactly as an odt's is. The root `AGENTS.md` flags that
  inconsistency with `html/pdf_file.cpp`'s scheme allowlist; this module is a
  second consumer of the document-link policy, not a third policy.
- **Horizontal rules, images and frontmatter** are not modelled yet — stages 4
  and 5 in [`PLAN.md`](PLAN.md).
