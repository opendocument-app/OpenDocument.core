# XML plan

Where an xml renderer would go, and in what order. Written before stage 1; keep
it honest as stages land.

## Today

`FileType::xml` exists (`file.hpp:150`, under the "Classification only"
comment) and carries a table row — `xml` extension, `application/xml` and
`text/xml`, `FileCategory::text`, `DocumentType::unknown`,
`{.detect_by_content = true}` (`file_type_table.cpp:664`).

Detection already works. `list_file_types` parses the file with
`util::xml::check_xml_file` and reports `[text_file, xml]`, plus
`scalable_vector_graphics` on top when the root element says so
(`open_strategy.cpp:296`). What is missing is everything after that:
`open_file_as` has no `FileType::xml` branch, and `open_file`'s unknown-type
path tries csv, json and svg before falling through to `text::TextFile`
(`open_strategy.cpp:432`). So a `.xml` decodes as `text_file` and renders
through `html::create_text_service` as a numbered line list.

Which is the whole problem. The xml files anyone opens on purpose —
`content.xml`, `document.xml`, anything a writer emitted rather than a human —
have no newlines in them, so the line list is one line several megabytes wide.

Already in place and reusable: `NoXmlFile` (`exceptions.hpp:146`), thrown by
`util::xml::parse` (`util/xml_util.cpp:18`); pugixml 1.15 as a dependency
(`conanfile.py:51`); and `internal/encoding` for transcoding.

## Target

An xml file opens as `XmlFile` and renders as a **source view**: indented,
syntax-highlighted, foldable, in one self-contained html document with no
JavaScript and no external resources. It stays a `TextFile`. The work is a
decoder shell plus one html service — no `Document`, no element adapters.

## Why not leave it to the browser

Every current browser ships an xml tree viewer, and none of them is reachable
from here. They fire on a *response* served as `text/xml`; odr serves nothing —
`HtmlService` hands the host an html document that a WebView displays, and
inside an html document the viewer never engages.

It would be the wrong lever even if it could be pulled. An
`<?xml-stylesheet?>` PI makes the browser run the XSLT instead of showing the
tree — silently rendering something else entirely. The viewers differ from each
other in folding, attribute display and error reporting. And the two engines
that matter most for this library, Android's WebView and WKWebView, are not the
browsers whose behaviour anyone checked.

## Decisions taken up front

**A file-level html service, not a document.** Xml has no document semantics.
Routing it through `ElementAdapter` would mean picking a `DocumentType` that is
a lie, and the generic renderer would have nothing to contribute — there is no
paragraph, no page, no sheet, only nesting. This is the opposite call from the
csv plan, and for a reason that transfers: a csv *is* a sheet, so the model
earns its keep and every binding gets a table for free. An xml file is not a
document that happens to be serialised as xml; it is the serialisation. So it
renders the way an image or a media file does — one `HtmlService`, html output
only, no element api. See also *the archive seam* below, which is where the
api-free choice does eventually cost something.

**`XmlFile` mirrors `JsonFile`.** `abstract::TextFile` over a
`std::shared_ptr<text::TextFile>`, constructed with the same probe-in-the-
constructor shape (`json/json_file.cpp:10`), `is_decodable()` false. The table
row flips to `{.detect_by_content = true, .open = true, .translate_html =
true}`.

The api-visible consequence: a `.xml` that reports `text_file` today will
report `xml`. No binding work — the enumerator already exists and the bindings
mirror the enum by ordinal — but a caller switching on `file_type()` sees the
change. That is the point of the change, and it is the same step csv and json
already took.

**The tree is not the bytes.** "We can already read it" is true only of the
parsed tree, and a pugixml tree is a normalisation of the file, not a view of
it. Lost, unavoidably: the original indentation, attribute quote style, whether
a character came in as `&#65;` or `A`, and — with `parse_eol` and
`parse_wconv_attribute`, both on by default — line-ending and in-attribute
newline spelling.

Accept that, deliberately. The alternative is a byte-faithful lexer over the
raw text, which keeps the author's formatting but does nothing for the minified
file that motivated the whole feature, and still has to reconstruct nesting
before it can fold anything. Pretty-printing is the feature; fidelity is the
price. If someone later wants a true "view source", it is a second mode over
the same css, not a redesign — noted under *Deferred*.

**Parse with `parse_full`, and keep whitespace-only text.** `util::xml::parse`
uses pugixml's defaults, which drop comments, processing instructions, the
declaration and the doctype — invisible in a viewer whose job is to show what
is in the file. `parse_full` is exactly those four added to `parse_default`.

`parse_ws_pcdata` is a separate flag and a separate question: keeping every
whitespace-only text node preserves fidelity but fills the tree with nodes we
are about to reindent anyway. Take `parse_ws_pcdata_single`, which keeps
whitespace-only text only where it is an element's sole child — so `<a>   </a>`
survives as content while the newline-and-tab between two sibling elements does
not. Note that this is the flag that makes the mixed-content rule below
decidable at all.

Do **not** reuse `util::xml::parse` for this; it hard-codes the default flags
and every existing caller wants them. Add the options at the xml module's own
call site.

**Encoding is declared in band, and pugixml will not honour it.** An xml file
names its own encoding in the declaration, which is better information than
`encoding::detect`'s uchardet guess over a 64 KiB probe. pugixml's
`encoding_auto` resolves UTF-8/16/32 from a BOM or the `<?xml` byte pattern
only; a `<?xml version="1.0" encoding="ISO-8859-1"?>` document is read as UTF-8
and yields invalid UTF-8 in the node strings, silently. (Verify against 1.15
before relying on the negative — but the design below is right either way.)

So: read the declaration's `encoding` pseudo-attribute from the head of the
file, map it through `text_encoding_by_name`, transcode with
`encoding::to_utf8`, and hand pugixml UTF-8. Precedence is declaration, then
BOM, then `text::TextFile::encoding()`'s guess. `XmlFile::encoding()` returns
the resolved value, so a caller can show it. An encoding we can name but not
decode throws `UnsupportedTextEncoding`, as the csv sheet path does and for the
same reason: the tree path has to produce UTF-8, and there is no "let the
browser sort it out" once the bytes are inside a parser.

**Mixed content is not reindented.** `<p>a <b>x</b> b</p>` carries significant
whitespace, and nothing short of a schema can tell it from the insignificant
kind. The rule: an element with any non-whitespace text child renders its
children inline, on one line, untouched; an element whose children are all
elements is indented and foldable. This is what every xml viewer does, it is
the one non-trivial rule in the writer, and it is the first thing a test should
pin.

**Highlighting is server-side spans.** One `<span class="odr-xml-…">` per
token, emitted by the writer. Not a JavaScript highlighter: the output has been
self-contained with no external resources since the css and js moved into the
document (`HtmlResource::is_shipped`, `html.hpp:51`), and shipping a
highlighter would reverse that for a job the writer is already doing as it
walks the tree.

Classes, following the `odr-text-*` naming in `frontend.cpp`: `odr-xml` on the
root, then `-tag`, `-name`, `-attr`, `-value`, `-text`, `-cdata`, `-comment`,
`-pi`, `-decl`, `-doctype`. Light palette only, as `text_css` is — a dark mode
is a question for every view at once, not for this one.

**Folding is `<details>`/`<summary>`, with no script.** The disclosure element
gets keyboard access, screen-reader semantics and — the reason it wins —
find-in-page that expands a collapsed section natively, which a `display:none`
toggle does not. Verify the Safari behaviour before promising it; Chrome and
Firefox have done it for years.

The layout objection is answerable: `details`/`summary` both `display:block`,
`summary::marker` removed via `list-style:none`, indentation carried inside the
summary so the `white-space:pre` flow stays intact. The start tag goes in the
`<summary>`, the children in the body, the end tag on a line after it.

The cost, recorded honestly: bulk expand-all/collapse-all needs JavaScript, so
stage 2 ships without it.

**No line numbers.** The text view has a numbered gutter (`html/text_file.cpp:101`).
Reproducing it here would number *our* lines, not the file's, which for a
reindented minified document is actively misleading. The gutter column goes to
the fold handles instead.

**Xml is the last resort in detection.** Insert the branch in `open_file`'s
unknown-type path *after* svg and before the text fallthrough
(`open_strategy.cpp:423`), so anything with a more specific reading keeps it.

Two behaviour changes fall out, both worth naming before they surprise someone.
A flat-xml ODF (`.fods` and friends) has no detection today — the flat mimetypes
are only aliases on the zip-backed rows (`file_type_table.cpp:26`) — so it
currently decodes as text and would now decode as xml. That is an improvement
(a source tree beats a single line) but it is not what a flat ODF should
eventually do, and it must not be mistaken for support. Likewise `.xhtml`,
`.rels`, `.plist` and every rss feed become source views rather than line
lists — correct for a source viewer, and correct that we do not try to *render*
xhtml.

**No DTD processing, and that is a feature.** pugixml does not resolve external
entities and does not expand internal entity declarations; it handles the five
predefined entities and numeric character references, and leaves `&foo;` as
literal text. For a viewer that opens files from the internet this closes XXE
and entity-expansion attacks by construction rather than by policy. The
fidelity note is the same sentence read the other way: an undefined entity is
shown as written, which for a source view is the right answer anyway.

**A parse failure falls back to text.** `XmlFile`'s constructor throws
`NoXmlFile`, `open_file` catches it and reaches `text::TextFile`, and a
malformed file renders as the line list it renders as today. Automatic and
correct — with one thing conceded up front: "show me the broken xml" is exactly
when a viewer is most wanted, and the tree path structurally cannot serve it.
That is the strongest argument for the byte-faithful second mode, and it is not
strong enough to build both now.

## Module layout

```
src/odr/internal/xml/
  xml_file.hpp/.cpp        abstract::TextFile over a text::TextFile; parse probe, encoding resolution
src/odr/internal/html/
  xml_file.hpp/.cpp        create_xml_service — the HtmlService and the writer
```

Both `.cpp` go into `ODR_SOURCE_FILES`: the html one next to
`html/text_file.cpp` (`CMakeLists.txt:148`), the module one after `util/` and
before `zip/` (`CMakeLists.txt:252`).

No `xml_util.cpp` — `internal/util/xml_util` is the shared xml helper and stays
where it is. Anything this module needs that is genuinely general (the
declaration sniff) belongs there, not in a second utility with the same name.

---

## Stage 1 — it opens, and it renders

The skeleton end to end, flat: highlighted and indented, not yet foldable.

- `XmlFile` per the `JsonFile` shape; declaration sniff, transcode,
  `parse_full | parse_ws_pcdata_single`; `encoding()` resolved as above.
- `open_file_as` gains a `FileType::xml` branch throwing `NoXmlFile`;
  `open_file` gains one after svg. Table row flipped to `{.open = true,
  .translate_html = true}`.
- `html/xml_file.cpp` with `create_xml_service`, and a `file_type()` branch in
  `html::translate(const TextFile &)` (`html.cpp:256`) so xml gets the tree and
  everything else keeps the line list. One view, `xml.html`, mirroring
  `html/text_file.cpp:26`.
- the writer: declaration, doctype, PI, comment, element, attribute, text and
  CDATA, escaped through `html::escape_text` / `escape_attribute`, indented,
  each token in its span. The mixed-content rule lands here, not later.
- `xml_css` in `frontend.cpp`, `write_xml_style` in `frontend.hpp`.
- `test/src/internal/xml/xml_file_test.cpp`, inline string literals in, html
  out (`text_file_test.cpp` is the shape). Minimum set: minified input
  reindents; mixed content does not; comments/PI/doctype/CDATA all survive;
  a declared non-UTF-8 encoding decodes; malformed input throws `NoXmlFile`.

`FileTypeCapabilities.declaration_matches_the_engines` (`odr_test.cpp`) opens
files per type from the test data against the row, so a handful of `.xml`
samples go into the test-data repo with this stage — see *Test data*.

## Stage 2 — folding

- `<details open>`/`<summary>` per element with element children, per the
  markup and css above. Elements with no children stay a plain line.
- everything open by default. Collapsing by default hurts find-in-page and
  hides the thing the user opened the file to see; the only case for it is
  size, which stage 3 handles with a threshold rather than a habit.
- the fold handle in the gutter column the line numbers do not occupy.

## Stage 3 — size

A `content.xml` is routinely tens of megabytes, and this path multiplies it:
pugixml's dom is roughly 1.5–2× the file plus the in-memory buffer, and a span
per token can be 5–10× the input in emitted html. Both land in a WebView on a
phone.

- a node budget in `HtmlConfig`, following `spreadsheet_limit`
  (`html.hpp:124`) — `std::optional<std::uint32_t> xml_node_limit`, `nullopt`
  for unlimited — and, past it, stop and emit a visible truncation notice
  rather than a silently short document.
- past a lower threshold, default the fold state to closed below some depth.
  This is the only case where collapsed-by-default is right, and it is a
  response to a measurement, not a preference.
- measure before choosing the numbers, on a real `content.xml`.

## Stage 4 — the archive seam

The filesystem view links every entry as an `application/octet-stream` data url
(`html/filesystem.cpp:110`), so browsing into a zip and looking at
`word/document.xml` downloads it. Routing entries through `html::translate`
instead is a separate feature with its own questions (which types, resource
paths, how deep), but it is the one that turns this from an xml-file viewer
into a way to inspect any office document's parts. Named here so the
dependency is on record; not scoped here.

## Deferred, by decision

- **Byte-faithful mode.** A lexer over the raw text, sharing the css, keeping
  the author's formatting and — the real payoff — able to render a malformed
  file up to the point where it breaks. Wanted; not wanted enough to build two
  renderers before one exists.
- **XSLT.** An `<?xml-stylesheet?>` PI is shown as the processing instruction it
  is. Applying it means an XSLT engine, which is larger than every format in
  this repository put together.
- **Rendering xhtml as html.** Same class of decision, and the answer is no for
  the same reason: this is a source viewer.
- **Namespace resolution.** pugixml does not process namespaces
  (`svg/svg_util.cpp:15` works around exactly this), and a source view should
  show the prefixes the file actually uses. Nothing to do.
- **Expand-all / collapse-all**, and **search within the tree** — both need
  JavaScript, and neither is worth being the reason this view starts shipping a
  script.
- **Dark mode**, which is a question for `frontend.cpp` as a whole.

## Test data

Content, not fixtures, for everything a string literal can express — the parser
and writer tests are inline. The test-data repo needs only what
`declaration_matches_the_engines` opens: a minified `content.xml` lifted from an
odt, a hand-formatted document with comments and a doctype, one non-UTF-8
declared encoding, and one file that is xml-shaped but malformed.
