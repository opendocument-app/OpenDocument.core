# AGENTS.md — `internal/rtf`

Rich Text Format, read as a text document. This file holds the rules of the
module. [`PLAN.md`](PLAN.md) holds the open work.

The spec is the RTF Specification 1.9.1 (March 2008) under
`offline/documentation/MSFT-RTF/`. It has no section numbers, so cite the
heading and the control word: *Conventions of an RTF Reader*, *Control Word*,
*Table Definitions*. `[MS-OXRTFEX]` and `[MS-OXRTFCP]` are different documents
and out of scope.

## Shape

```
bytes ─▶ Tokenizer ─▶ TreeBuilder ─▶ ElementRegistry ─▶ Document ─▶ RtfFile
         (rtf_tokenizer)  (rtf_parser)                  (rtf_document)  (rtf_file)
```

| File | What |
|------|------|
| `rtf_token.hpp` | The `Token` variant: `GroupOpen`, `GroupClose`, `ControlWord`, `ControlSymbol`, `HexEscape`, `Text`, `Binary`, `End`. |
| `rtf_tokenizer.*` | Bytes to tokens. Knows nothing about groups or destinations. |
| `rtf_state.*` | The group stack: `{` saves, `}` restores. `max_depth` bounds it. |
| `rtf_parser.*` | `parse_tree`: tokens to `root → (paragraph \| page_break) → (text \| line_break)`. |
| `rtf_element_registry.*` | `internal::ElementRegistry` plus one `Text` payload. The smallest registry in the tree. |
| `rtf_document.*` | `internal::Document` plus the element adapter. Every style hook returns a default. |
| `rtf_file.*` | `abstract::DocumentFile`. Validates the magic and hands out the document. |

An rtf is one byte stream, so `internal::Document` gets a null
`ReadableFilesystem`, as `csv` does.

## What it decodes

Paragraph structure and text: `\par`, `\line`, `\tab`, `\page`, `\sect`, the
literal-character control words (`\emdash`, `\bullet`, the quotes), the escapes
`\\` `\{` `\}` `\~` `\_` `\-`, `\'hh` in the run's encoding, and `\uN` with
surrogate pairs. The encoding comes from `\ansi`, `\mac`, `\pc`, `\pca` and
`\ansicpgN` through `internal/encoding`. A bare CR or LF is not text.

Every other control word is ignored. That is the spec's rule for a reader that
meets a control word it does not know. Formatting, page layout, tables and
pictures are the stages in `PLAN.md`.

## Rules

- **Leniency is the spec here and does not violate the fail-fast rule.**
  Unknown control words are ignored, `{\*` groups with an unknown destination
  are discarded, an unmatched `}` is ignored. What throws: a group left open at
  EOF, an invalid hex digit after `\'`, a `\binN` past EOF, a trailing `\`, and
  nesting past `State::max_depth`.
- **The tokenizer reads `\binN`, not the parser.** The payload is raw bytes
  that can contain braces, so a brace-counting scan over it desyncs the group
  nesting. That is why a discarded destination is still tokenized.
- **Text is bytes until the run ends.** `\'hh` yields one byte. In a
  double-byte run two escapes are one character. The accumulator is `m_bytes`
  plus the run's `TextEncoding`, decoded through `encoding::to_utf8` at a
  flush. An undecodable run degrades per byte: ascii passes, a byte at or above
  0x80 becomes U+FFFD.
- **`\uN` is a signed UTF-16 code unit.** `U+F020` arrives as `\u-4064`, so
  fold with `+ 0x10000` before the surrogate test. A code point above the BMP
  arrives as two `\uN`. A high surrogate waits for its low one. An unpaired
  surrogate becomes U+FFFD. `\u0` is dropped, because a parameterless `\u`
  folds to it and a NUL byte would reach the html.
- **`\ucN` counts a control word or symbol as one character**, a `\binN` with
  its payload included. A group boundary cancels a pending skip.
- **`HexEscape` is its own token.** `\ucN` counts an escape as one character,
  where a text run counts bytes.
- **`\page` emits `ElementType::page_break`** as a child of root, as
  `oldms/text` does. It closes an open paragraph without opening a new one, as
  `\sect` and the end of the file do. Writers emit `\par\page`, so a new
  paragraph there would blank-line every page break. Only `\par` and `\row`
  create an empty paragraph.
- **`\cell` renders as a tab and `\row` as a paragraph end** until tables
  land. This keeps table text readable.
- **A `{\*` destination needs no entry in the discard table.** The table in
  `rtf_parser.cpp` catches the destinations that writers leave unmarked:
  `\fonttbl`, `\colortbl`, `\info`, `\pict`, `\fldinst`, and `\nonshppict`, the
  unmarked twin of `{\*\shppict}` that would emit every image twice. A field
  keeps its cached `\fldrslt` text only.

## Testing

Every test is an inline string literal. There is no `.rtf` under `test/data`.
`test/src/internal/rtf/rtf_tokenizer_test.cpp` covers the delimiter rules
token by token. `rtf_document_test.cpp` runs `parse_tree` and flattens the
tree to one line (`P(…)`, `|`, `PB`).
