# AGENTS.md — `internal/xml`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file covers what
xml does differently, and why. What is not built yet is in [`PLAN.md`](PLAN.md).

## A source view, not a document

Xml has no document semantics: no paragraph, no page, no sheet, only nesting.
An `ElementAdapter` would mean picking a `DocumentType` that is a lie for a
renderer that has nothing to contribute. So `XmlFile` mirrors `JsonFile` — an
`abstract::TextFile` over a `text::TextFile`, probe in the constructor,
`is_decodable()` false — and renders through one `HtmlService`.

It stays a text file, so `TextFile::text()` keeps working on it.

The browser's own xml viewer is not an option: it fires on a *response* served
as `text/xml`, and `HtmlService` hands the host an html document.

## The dispatch is on the decoded file

`html::translate(const DecodedFile &)` sends `FileType::xml` to
`create_xml_service`, next to the csv branch and for the same reason: a line
list is never what a viewer wants from a file with no line breaks in it.
`html::translate(const TextFile &)` is left alone — asking for the text
rendering gets the text rendering.

The service takes the public `odr::TextFile` and casts its impl back to
`XmlFile` — the parse is the file's, not the writer's, so the writer never gets
to parse something the decoder did not accept.

## The tree is not the bytes

A pugixml tree is a normalisation. Lost: the original indentation, attribute
quote style, whether a character arrived as `&#65;` or `A`, and — `parse_eol`
and `parse_wconv_attribute` are both on — line-ending and in-attribute newline
spelling. Pretty-printing is the feature; fidelity is the price.

One cost worth naming: a **malformed** file has no tree, so `XmlFile` throws
`NoXmlFile` and it falls back to the line list — which is exactly when a viewer
is most wanted.

## The parse flags, and where they live

`parse_source` (`xml_file.cpp`, file-local) is the only place they are written,
and `XmlFile`'s constructor is its only caller. Not `util::xml::parse`, whose
every other caller wants pugixml's defaults.

- `parse_full` adds the four node kinds `parse_default` drops — comments,
  processing instructions, the declaration, the doctype.
- `parse_ws_pcdata_single` keeps whitespace-only text where it is an element's
  only child, so `<a>   </a>` survives while the newline between two siblings
  does not. It is what makes the mixed-content rule decidable.

**No DTD processing, and that is a feature.** pugixml resolves no external
entities and expands no internal ones, which closes XXE and entity expansion by
construction. An undefined entity is shown as written, which a source view
wants anyway.

## The encoding is declared in band

pugixml's `encoding_auto` resolves UTF-8/16/32 from a BOM or the `<?xml` byte
pattern only, so `encoding="ISO-8859-1"` is read as UTF-8 and yields invalid
UTF-8 in the node strings, silently.

`util::xml::read_declared_encoding` reads the pseudo-attribute off the head of
the file, `text_encoding_by_name` maps it, and the bytes are transcoded before
pugixml sees them. Precedence: declaration, BOM, detected guess. The sniff is
ascii-only, which loses nothing — utf-16 and utf-32 are named by their BOM.

An encoding we can name but not decode throws `UnsupportedTextEncoding`: there
is no "let the browser sort it out" once bytes are inside a parser.

## Mixed content is not reindented

Nothing short of a schema tells significant whitespace from the other kind. So
**an element with any text child renders its children inline, on one line,
untouched**; an element whose children are all elements is indented and
foldable. The one non-trivial rule in the writer, and the first thing the tests
pin.

*Any* text child, not any non-whitespace one: with `parse_ws_pcdata_single` the
two differ only for `<a>   </a>`, and the looser rule leaves it alone.

## Writing decisions

- **Highlighting is server-side spans**, one per token, emitted as the writer
  walks the tree. A JavaScript highlighter would undo the self-contained output
  for a job the writer already does.
- **Folding is `<details>`/`<summary>`, and nothing drives it** — keyboard
  access, screen-reader semantics, and find-in-page that natively expands a
  collapsed section. Start tag in the `<summary>`, children then end tag in the
  body, so collapsing hides the whole node. Everything is open by default. Bulk
  expand-all/collapse-all would need JavaScript of its own, and there is none.
  The view's only script is the shared `search.js`, which opens the section a
  hit is in.
- **No line numbers**; the column carries the fold handles, and every line
  reserves it so folding does not shift siblings.
- **Indentation is spaces, not padding**, so a copy of the page carries it.
- **Not `html::escape_text`** — it folds spaces into `&nbsp;` and tabs into
  `&emsp;`. `escape_source` in `html/xml_file.cpp` escapes `&`, `<` and `>`.
- **Attribute values take whichever quote needs no entity**, and where the
  value carries both, the double quote becomes `&quot;`. Their whitespace is
  source text like any other, so the span preserves it.

## One parse, and the file holds it

`XmlFile`'s constructor parses, and keeps the tree; `document()` hands out a
`const &` and the html service walks it per render. Recognising the file and
rendering it are the same parse.

Two things ruled the alternatives out. pugixml is dom-only — no sax, no
incremental mode — so there is no validating the head of a file the way csv's
`probe` scores its opening bytes; and a source view's whole contract is that
what opened will render, which a sniff gives up. And a document cannot be
copied out (`reset(proto)` is a deep clone, no cheaper than reparsing), so
lending it beats returning it.

The price is memory: the dom is roughly twice the file, held for as long as the
`XmlFile` is, whether or not anyone renders it. `PLAN.md`'s size section owns
that number.

## Detection

Xml is the **last resort** in `open_file`'s unknown-type path, after csv, json
and svg.

Two consequences. A flat-xml ODF (`.fods` and friends) has no detection — the
flat mimetypes are only aliases on the zip-backed rows — so it decodes as xml.
Better than text, **not** flat-ODF support. Likewise `.xhtml`, `.rels`,
`.plist` and rss feeds become source views: correct for a source viewer, and
correct that we do not try to *render* xhtml.
