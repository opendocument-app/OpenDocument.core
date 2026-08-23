# AGENTS.md — `internal/rtf`

Rich Text Format, read as a text document. Read [`PLAN.md`](PLAN.md) first: it
carries the staging, the decisions taken up front and what each later stage
owes. This file records what stage 1 actually built and where it deviates.

Spec references are the RTF Specification 1.9.1 (March 2008). It has no section
numbers, so cite the heading and the control word — *Conventions of an RTF
Reader*, *Control Word*, *Table Definitions*. Not `[MS-OXRTFEX]` /
`[MS-OXRTFCP]`, which are different documents and out of scope.

## Shape

```
bytes ─▶ Tokenizer ─▶ TreeBuilder ─▶ ElementRegistry ─▶ Document ─▶ RtfFile
         (rtf_tokenizer)  (rtf_parser)                  (rtf_document)  (rtf_file)
```

| File | What |
|------|------|
| `rtf_token.hpp` | The `Token` variant the tokenizer yields. |
| `rtf_tokenizer.*` | Bytes → tokens. Knows nothing about groups or destinations. |
| `rtf_state.*` | The group stack: `{` saves, `}` restores. |
| `rtf_parser.*` | `parse_tree` — tokens → `root → paragraph → (text \| line break)`. |
| `rtf_element_registry.*` | The flat registry, copied from `oldms/text` minus its style index. |
| `rtf_document.*` | `internal::Document` + the element adapter. |
| `rtf_file.*` | `abstract::DocumentFile`; validates the magic, hands out the document. |

There is no filesystem: an rtf is one byte stream, so `internal::Document` gets
a null `ReadableFilesystem` the way `csv` does.

## What stage 1 decodes

Paragraph structure and text: `\par`, `\line`, `\tab`, `\page`, `\sect`, the
literal-character control words (`\emdash`, `\bullet`, the quotes, …), the
escapes `\\` `\{` `\}` `\~` `\_` `\-`, `\'hh` in the run's encoding, and `\uN`
including surrogate pairs. The encoding comes from `\ansi` / `\mac` / `\pc` /
`\pca` / `\ansicpgN` through `internal/encoding`.

Everything else is ignored, which is the spec's own rule for a reader meeting a
control word it does not know. Character and paragraph formatting, tables,
pictures and lists are stages 2–5 in `PLAN.md`.

## Rules worth knowing before touching this

- **Leniency is the spec here, and does not violate the root `AGENTS.md`
  fail-fast rule.** Unknown control words are ignored, `{\*` groups whose
  destination we do not implement are discarded, an unmatched `}` is ignored.
  What *does* throw: a group left open at EOF, an invalid hex digit after `\'`,
  a `\binN` running past EOF, a trailing `\`, and nesting past `State`'s depth
  bound.
- **`\binN` is read by the tokenizer, not the parser.** Its payload is raw
  bytes that may contain braces and backslashes, so a brace-counting scan over
  them would desync the group nesting — including inside a group being
  discarded. That is why skipping an ignorable destination still tokenizes.
- **Text is bytes until the run ends.** `\'hh` yields one *byte*: in a
  double-byte run two consecutive escapes are one character. The accumulator is
  `m_bytes` plus the run's `TextEncoding`, decoded through `encoding::to_utf8`
  only at a flush. Decoding per escape corrupts every multibyte run.
- **`\uN` is a UTF-16 code unit, signed.** `U+F020` arrives as `\u-4064` (fold
  with `+ 65536` *before* the surrogate test), and anything above the BMP
  arrives as a surrogate pair across two `\uN`. A high surrogate is held pending
  and combined with the low one; unpaired, it becomes U+FFFD.
- **`\ucN` counts control words and symbols as one character each**, strictly,
  a `\binN` and its payload included. Real writers always emit the fallback, so
  the strict reading costs nothing and is what the spec says. A group boundary
  cancels a pending skip.
- **A `{\*`-marked destination needs no entry in the discard table**; the `\*`
  control symbol discards whatever follows it. The table in `rtf_parser.cpp` is
  only for destinations that are *not* marked — `\fonttbl`, `\colortbl`,
  `\info`, `\pict`, and notably `\nonshppict`, the unmarked twin of
  `{\*\shppict}` that would otherwise emit every image a second time.

## Deviations from `PLAN.md`

- **`HexEscape` is its own token**, not folded into `Text`. The plan's variant
  had no place to put the decoded byte of a `\'hh`, and `\ucN` counts an escape
  as one character where a text run counts bytes.
- **`\page` emits `ElementType::page_break`** (a child of root, as `oldms/text`
  does) even though the html renderer ignores that type today.
- **`\cell` renders as a tab and `\row` as a paragraph end.** The plan defers
  tables to stage 4; until then this keeps table text readable rather than
  running it together.
- **An undecodable run degrades per byte**, not per run: ascii passes through
  and only bytes ≥ 0x80 become U+FFFD. The plan said the whole run degrades,
  which loses the ascii skeleton for no reason.

## Testing

Everything is inline string literals — an rtf fragment is readable in a raw
string, and there is no `.rtf` anywhere under `test/data`. `rtf_tokenizer_test`
covers the delimiter rules token by token; `rtf_document_test` runs
`parse_tree` and flattens the tree to one line (`P(…)`, `|`, `PB`).

A render test against a real fixture needs a file committed to
`test/data/input` plus the reference-output regen, and is owed once stage 5
lands a picture that cannot be written inline.
