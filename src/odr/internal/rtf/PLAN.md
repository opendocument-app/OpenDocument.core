# RTF plan

Where an rtf module would go, and in what order. Written before stage 1; keep
it honest as stages land.

## Today

**Stage 1 has landed.** An rtf opens as a text document and renders: the
tokenizer, the group/destination machinery and plain text with paragraphs, line
breaks, tabs and the full `\'hh` / `\uN` decoding. The table row now declares
`DocumentType::text` and `{.open, .translate_html, .color_scheme}`, and
`open_strategy` has its three branches. See [`AGENTS.md`](AGENTS.md) for what
was built and where it deviates from what follows.

Stages 2–5 below are untouched: no character or paragraph formatting, no page
layout, no tables, no pictures.

The public enum is already mirrored by the bindings (`bind_file.cpp:39`,
`ODRFile.mm:43`), so **no binding work is needed for any stage below** — the
ordinal is spent, and everything else arrives through the abstract document
model the generic renderer already walks.

## Spec

`offline/documentation/MSFT-RTF/MSFT-RTF-080320/` — the RTF Specification
version 1.9.1 (March 2008, the final one). Cite section names in code, the way
`oldms/` cites `[MS-DOC] §2.4.1`; RTF's spec is not numbered, so cite the
heading (e.g. *Conventions of an RTF Reader*, *Table Definitions*, *Pictures*)
and the control word.

Not the same document as `[MS-OXRTFEX]` (Exchange's HTML-in-RTF encapsulation)
or `[MS-OXRTFCP]` (RTF compression) — neither is in scope.

## Target

An rtf opens as a text document — `root → paragraph → span → text`, tables and
frames where the file has them — so the generic HTML renderer and every binding
get it without format-specific code. `DocumentType::text`, `open` and
`translate_html` in the table row; `is_editable()` false, `save` throws, exactly
as `oldms/text`.

## Decisions taken up front

**Mirror `pdf`, not `oldms`.** RTF is a *text* format: `{`, `}`,
`\controlword<param>`, `\'hh`, and literal bytes. None of `oldms/text`'s
machinery applies — no CFB container, no FIB, no piece table, no
`PlcBteChpx`→`ChpxFkp` walk, no packed structs, no host-endianness caveat. What
does apply is `internal/pdf`'s parser shape, and it applies unusually well:

- **`RtfTokenizer` follows `pdf::ObjectParser`** (`pdf_object_parser.hpp`): hold
  `std::istream *` + `std::streambuf *`, prepare the stream once with an
  `std::istream::sentry` in the constructor, and expose
  `getc`/`bumpc`/`bumpnc(n)`/`ungetc` that throw `std::runtime_error` on
  unexpected exhaust. This is not decoration: `\binN` needs a raw n-byte read
  mid-stream (`bumpnc`), and `\'hh` needs the same `hex_char_to_int` /
  `two_hex_to_char` statics. Those two helpers are six lines — **duplicate
  them**, do not reach into `internal/pdf`. (The csv plan's rule: anything
  genuinely shared moves out to its own package first, and two hex helpers do
  not earn a package.)
- **`read_token()` returns a variant, like `GraphicsOperatorParser::read_operator()`**
  — `GroupOpen`, `GroupClose`, `ControlWord{name, optional<int32> parameter}`,
  `ControlSymbol{char}`, `Text{bytes}`, `Binary{bytes}`, `End`. The spec's
  Appendix A sample reader is a callback design over globals; a pull-based token
  stream is what the rest of this codebase looks like and is what makes the
  tokenizer testable on inline strings.
- **`RtfState` follows `pdf::GraphicsState`** (`pdf_graphics_state.hpp`):
  `std::vector<State> stack` that is never empty, `save()` on `{`, `restore()`
  on `}`, and a `ContentScope`-style RAII guard for destination groups. The
  spec's *Conventions of an RTF Reader* prescribes precisely this ("stores its
  current state on the stack … retrieves the current state from the stack"), and
  `pdf` already carries the leniency precedent — "an unmatched `Q` is ignored:
  the state the running content stream started from always remains".

**Leniency is the spec, and is therefore not a violation of fail-fast.** The
root `AGENTS.md` says throw where the spec dictates what to expect. Here the
spec dictates the opposite: an unknown control word "should be ignored", and
`{\*` opens an ignorable destination the reader "should discard all text up to
and including the closing brace" of. A reader that throws on an unknown control
word cannot read any file written by a newer writer, which is the stated design
goal of `\*`. So:

- **Ignore** unknown control words and control symbols; **skip** `{\*` groups
  whose destination we do not implement; ignore an unmatched `}`.
- **Throw** where the spec does dictate: a group left open at EOF, an invalid
  hex digit after `\'`, `\binN` running past EOF, nesting past a depth bound
  (borrow `ObjectParser::NestingGuard` — the group stack is heap, but the
  destination skip is recursive).
- The one place the spec *invites* rejection is `\cellxN` outside a table
  ("probably created maliciously"). Not worth a throw at our fidelity; note and
  ignore.

**Text is bytes until a run ends.** `\'hh` yields one *byte*, not one character:
in a Shift-JIS run two consecutive `\'hh` escapes are one character. So the
accumulator is a byte buffer plus the run's `TextEncoding`, flushed through
`encoding::to_utf8` when the encoding changes, the formatting changes, or the
paragraph ends. Decoding per escape would corrupt every multibyte run and is the
single easiest mistake to make here.

`\uN` is different — it bypasses the byte buffer (flushing it first, to keep
order). Three traps:

- N is a **UTF-16 code unit, not a code point**, and anything above the BMP
  arrives as a *surrogate pair*: two consecutive `\uN`, each with its own `\ucN`
  fallback in between. So a high surrogate is held pending and combined with the
  low one that follows; only the combined code point goes through
  `util::string::append_c32`. Appending each half on its own turns every emoji
  into two replacement characters. An unpaired surrogate still standing at a
  flush is U+FFFD.
- N is *signed 16-bit*, so `U+F020` arrives as `\u-4064` and must be folded back
  with `+ 65536` — before the surrogate test, which is what the folded value is
  for.
- `\ucN` gives the number of following characters to skip as the ANSI fallback,
  is scoped like a character property (so it lives in `RtfState`, restored by
  `}`), defaults to 1, and counts *any* control word or symbol as one
  character — with a `\binN` and its payload counting as one.

**Encoding comes from `internal/encoding`.** `\ansi`/`\mac`/`\pc`/`\pca` set a
document default, `\ansicpgN` overrides it, the font table's `\fcharsetN` (or
`\cpgN`, which supersedes it) sets it per font, and `\fN` selects the font. That
resolution chain lands on a `TextEncoding` and the transcode is
`encoding::to_utf8`. The single-byte tier covers what real-world rtf uses;
`\fcharset128`/`129`/`134`/`136` (Shift-JIS, EUC-KR, GB2312, Big5) hit the
"named but not decoded" tier, so their `\'hh` runs degrade to U+FFFD — acceptable,
because a writer emitting CJK generally emits `\uN` alongside, which is decoded
regardless of the run's encoding.

**Flat `ElementRegistry`, copied not shared.** Unlike csv, an rtf's element
count is bounded by its paragraph count, so the pattern from the root
`AGENTS.md` applies unchanged and `oldms/text/doc_element_registry.*` is the
template. Every engine has its own registry (`doc`, `ppt`, `xls` all do); copy
it rather than inventing a shared one.

**No writer.** RTF is the one format here that would be pleasant to *emit*, and
`back_translate` exists. Out of scope, in every stage.

---

## Stage 1 — plumbing, tokenizer, plain text — **done**

The narrowest thing that renders. No formatting at all beyond paragraph
structure, so the tokenizer and the group machinery can be proven before
anything is layered on them.

- `rtf_tokenizer.{hpp,cpp}` as above, with the delimiter rules from *Control
  Word* exactly: a control word is `\` + ASCII letters, terminated by a space
  (consumed), by a digit or `-` (a parameter of up to 10 digits follows), or by
  any other character (not consumed). **The parameter's terminator is a
  delimiter under the same rule** — a space there is consumed, anything else is
  left unread. So `\fs24 Text` starts its text at `T`, not at a space, and
  `\bin4 ` puts the four raw bytes immediately after the consumed space;
  swallowing that space is the difference between correct text and a payload
  read shifted by one. A control *symbol* is `\` + one non-letter and takes no
  delimiter — a space after it is text.
- `rtf_state.{hpp,cpp}`: the group stack, the current destination, and the
  character/paragraph property structs (empty but for `\uc` in this stage).
- destination handling: a table of known destinations; `{\*\unknown` skips to the
  matching `}`. The skip must **still tokenize** — a `\binN` inside a skipped
  group carries raw bytes that can contain braces, so a brace-counting scan over
  raw bytes desyncs. Same reason `\'7b` must not be mistaken for a group open.
- text: `\par` ends a paragraph, `\line` a line break, `\tab` a tab, `\page` a
  page break; `\\`, `\{`, `\}` are literal; a bare CR/LF is *not* text and is
  dropped; `\~` non-breaking space, `\_` non-breaking hyphen, `\-` optional
  hyphen dropped. (`oldms/text`'s `TextCleaner` is the same job with different
  spellings.)
- fields keep the cached result only, the position `.doc` takes: the
  ignorable-destination rule hides `{\*\fldinst}` and leaves `\fldrslt`'s text
  flowing. *(Landed with `fldinst` in the discard table as well — not every
  writer marks it.)*
- `rtf_file.{hpp,cpp}` — `RtfFile : abstract::DocumentFile` over a plain
  `abstract::File`, shaped like `pdf::PdfFile` (`pdf_file.hpp:16`); a branch in
  both `open_strategy::open_file` and `open_document_file`; `types_by_content`
  needs nothing, magic already reports the type.
- `file_type_table.cpp:390` → `DocumentType::text`, `.open = true`,
  `.translate_html = true`. `odr_test` fails if declared capabilities exceed
  what the engine does, so this row moves stage by stage.
- `CMakeLists.txt` `ODR_SOURCE_FILES`.

**Tests** are inline string literals against the tokenizer and against a small
whole document — the delimiter rules (space eaten after a control word *and*
after its parameter, digit kept, `\b0` vs `\b`), `\'hh` in a windows-1252 run,
`\uN` negative folding, a non-BMP character as a surrogate pair across two
`\uN`, an unpaired surrogate, `\ucN` skipping across a control word, an
unmatched `}`, an unterminated group, and a `\bin` payload containing `}`.

## Stage 2 — character formatting

- `{\fonttbl{\fN\fcharsetN Name;}…}` and `{\colortbl;\redN\greenN\blueN;…}` —
  both are destinations, both parsed into a `StyleRegistry` alongside the
  element registry, mirroring `oldms/text/doc_style.*`. Note the colour table's
  leading `;`: entry 0 is the "auto" colour and is empty.
- `\b \i \ul \ulnone \strike \scaps \caps \sub \super \nosupersub`, `\fN`,
  `\fsN` (half-points), `\cfN`/`\cbN`, `\highlightN`, `\plain` (reset) →
  `TextStyle`, resolved to spans exactly as `.doc` does: equal property sets
  share one resolved style, adjacent equal-style runs merge.
- font names interned in the `StyleRegistry` and never mutated afterwards, so
  `TextStyle::font_name` (`const char *`) stays valid — the `.doc`/`.xls` rule.

## Stage 3 — paragraph, section, page

- `\pard` reset, `\ql \qc \qr \qj`, `\liN \riN \fiN`, `\sbN \saN \slN` →
  `ParagraphStyle`. Twips throughout; `Measure` already carries units.
- `\paperwN \paperhN \marglN \margrN \margtN \margbN \lndscpsxn` →
  `PageLayout` off `TextRootAdapter::text_root_page_layout`, which stage 1
  returns empty from.
- `\sect`/`\sectd` — with one page layout there is nowhere to put a second
  section, so treat a section break as a page break and take the first section's
  geometry. Honest and enough; revisit only if a reference document needs it.
- lists: `{\listtext …}` and `{\pntext …}` carry the rendered bullet or number.
  Emitting them as literal text is right at this fidelity and costs nothing;
  real `ListItem` elements need `{\*\listtable}` + `{\*\listoverridetable}` +
  `\lsN\ilvlN` resolution and belong in a later stage, not here.

## Stage 4 — tables

The largest single piece, and the one place RTF is genuinely awkward.

*Table Definitions*: there is no table group. A row is a run of paragraphs each
carrying `\intbl`, cells terminated by `\cell`, the row by `\row`, and the
geometry given by a `<tbldef>` — `\trowd` followed by one `\cellxN` per cell,
where N is that cell's cumulative **right edge** in twips.

- **Buffer the row; do not stream it.** The spec is explicit that Word 97 wrote
  `<tbldef>` before the cells, Word 2002–2007 write it *after* (and repeat it
  before), and the grammar admits all three orders. So accumulate cells into a
  pending row and apply whichever `<tbldef>` was seen when `\row` fires — and
  make a second, identical `\trowd` idempotent rather than a second row.
- column widths are differences of successive `\cellxN`; `\trleftN` is the row's
  left edge and the first difference is measured from it.
- `\clmgf`/`\clmrg` (horizontal) and `\clvmgf`/`\clvmrg` (vertical) merges map
  onto `TableCellAdapter::table_cell_span` plus `table_cell_is_covered` — the
  `f` variants start a merged region, the bare ones continue it, so the span is
  only known once the run ends. Another reason the row is buffered.
- consecutive rows sharing a `<tbldef>` are one table; a row whose cell edges
  differ starts a new one. `\itapN` gives the nesting depth, with
  `\nestcell`/`\nestrow` and `{\*\nesttableprops}` for the inner levels —
  implement depth 0 first and treat deeper rows as their own table, then nest.
- borders and shading (`\clbrdr*`, `\trbrdr*`, `\clcbpat`) into `TableStyle` /
  `TableCellStyle` last; they are independent of the reconstruction.

## Stage 5 — images

- `{\pict …}` is a destination whose payload is hex `#SDATA` by default or
  `\binN` + raw `#BDATA`. Decode to a `std::string`, wrap in
  `common::MemoryFile` (`common/file.hpp:34`), hand back through
  `ImageAdapter::image_file` with `image_is_internal() == true`.
- `\pngblip` and `\jpegblip` render. `\emfblip`, `\wmetafileN`, `\macpict`,
  `\pmmetafileN`, `\dibitmapN`, `\wbitmapN` have no decoder here
  (`windows_metafile` / `enhanced_metafile` are classification-only file types),
  so **drop the frame** rather than emit a broken `<img>` — "pass through what we
  don't model". Word-97-era files lean on `\wmetafile8` heavily; anything modern
  is png/jpeg.
- **`\nonshppict` must be skipped explicitly.** The pair is
  `{\*\shppict{\pict …}}{\nonshppict{\pict …}}` — the second group is the same
  image again, for readers that cannot do `\shppict`, and it is *not* `\*`-marked,
  so the ignorable-destination rule will not hide it. Miss this and every image
  appears twice.
- `\picwgoalN`/`\pichgoalN` are the display size in twips → `Frame` measures;
  `\picwN`/`\pichN` are the intrinsic pixel (or metafile extent) size and are the
  fallback. `\picscalexN`/`\picscaleyN` are percentages applied on top.
- shapes (`{\shp{\*\shpinst{\sp{\sn pib}{\sv …}}}}`) hold pictures too, and
  `{\object\objemb}` holds embedded OLE. Both out of scope; both are skipped
  cleanly by the destination rule if left unimplemented.

---

## Deferred, by decision

- **Headers, footers, footnotes, endnotes, comments** — each its own destination
  (`\header*`, `\footer*`, `\footnote`, `\annotation`). Cheap to reach once the
  destination table exists, but there is nowhere in the element model to put
  them today; `.doc` drops them for the same reason.
- **Real list items** — see stage 3.
- **Style sheet** (`{\stylesheet}`) and `\sN`/`\csN` inheritance. Direct
  formatting first; this is the same layer `.doc` still misses (§2.4.6.5 there).
  Worth more here than in `.doc`, because rtf writers lean on styles for heading
  fonts.
- **Math** (`{\mmath …}`), **drawing objects**, **bidi** (`\rtlch`/`\ltrch` and
  the associated-font `\af*` chain), **revision marks**, **East Asian composite
  fonts**.
- **Multibyte `\'hh` runs** — blocked on the multibyte tier in the encoding
  package, and largely masked by `\uN` in practice.

## Test data

There is no `.rtf` anywhere under `test/data`, and that tree is fetched from the
pinned repos rather than vendored. Stage 1–4 parser tests are inline string
literals (which is the better test anyway — an rtf fragment is readable in a
`R"(...)"`), but a render test needs a real fixture committed to
`test/data/input` plus the reference-output regen. Stage 5 needs one either way,
since a picture cannot reasonably be written inline.
