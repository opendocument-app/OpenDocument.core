# AGENTS.md — `internal/xml`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file covers what
xml does differently, and why. Open work is in [`PLAN.md`](PLAN.md).

## Three things live here

- `xml_util` is the shared plumbing: `parse`, `escape_text`,
  `escape_attribute`, `read_declared_encoding`, `tokenize_text`. odf, ooxml,
  svg and the html writer go through it. It depends on no other engine.
- `xml_tree_edit` is the node editing the odf and ooxml write sides share.
- `xml_file` is the format: xml opened as a file of its own and rendered as a
  source view. The rest of this file is about it.

## A source view, not a document

Xml has no document semantics, only nesting. So `XmlFile` mirrors `JsonFile`:
an `abstract::TextFile` over a `text::TextFile`, `is_decodable()` false, and
one `HtmlService`. `TextFile::text()` keeps working on it.

`html::translate(const DecodedFile &)` sends `FileType::xml` to
`create_xml_service`. `html::translate(const TextFile &)` stays the line list.
The service casts the public `odr::TextFile` back to `XmlFile`, so the writer
renders the tree the decoder accepted and never parses on its own.

## The tree is not the bytes

A pugixml tree is a normalisation. Lost: indentation, attribute quote style,
whether a character arrived as `&#65;` or `A`, and line-ending spelling
(`parse_eol` and `parse_wconv_attribute` are on). Pretty-printing is the
feature. A malformed file has no tree, so `XmlFile` throws `NoXmlFile` and the
file falls back to the line list.

## The parse flags

`parse_source` (`xml_file.cpp`, file-local) is the only place that sets them,
and `XmlFile`'s constructor is its only caller. `xml::parse` keeps pugixml's
defaults for every other caller.

- `parse_full` adds comments, processing instructions, the declaration and
  the doctype.
- `parse_ws_pcdata_single` keeps whitespace-only text where it is an
  element's only child, so `<a>   </a>` survives while the newline between
  two siblings does not.

pugixml resolves no external entities and expands no internal ones. That
closes XXE and entity expansion by construction. An undefined entity is shown
as written.

## The encoding is declared in band

pugixml's `encoding_auto` resolves UTF-8/16/32 from a BOM only, so
`encoding="ISO-8859-1"` yields invalid UTF-8 silently. `read_declared_encoding`
reads the pseudo-attribute off the head of the file, `text_encoding_by_name`
maps it, and the bytes are transcoded before pugixml sees them. Precedence:
declaration, BOM, detected guess. An encoding we can name but not decode
throws `UnsupportedTextEncoding`.

## Mixed content is not reindented

Nothing short of a schema tells significant whitespace from the other kind. So
an element with any text child renders its children inline on one line,
untouched. An element whose children are all elements is indented and
foldable. This is the one non-trivial rule in the writer, and the first thing
the tests pin.

## Writing decisions

- Highlighting is server-side spans, one per token, emitted as the writer
  walks the tree.
- Folding is `<details>`/`<summary>`. The start tag is the summary, the
  children and the end tag are the body, so a collapsed node hides whole.
  Everything is open by default. The view's scripts are the shared
  `search.js`, which opens the section a hit is in, and `viewport.js`.
- No line numbers. The column carries the fold handles, and every line
  reserves it so folding does not shift siblings.
- Indentation is spaces, not padding, so a copy of the page carries it.
- Not `html::escape_text`, which folds spaces into `&nbsp;`. `escape_source`
  in `html/xml_file.cpp` escapes `&`, `<` and `>`.
- An attribute value takes whichever quote needs no entity. If it carries
  both, the double quote becomes `&quot;`.

## One parse, and the file holds it

`XmlFile`'s constructor parses and keeps the tree. `document()` hands out a
`const &`. pugixml is dom-only, so there is no bounded probe like csv's, and a
source view's contract is that what opened will render. The price is memory:
the dom is about twice the file, held as long as the `XmlFile` lives.
`PLAN.md` owns that number.

## Detection

Xml is the last try in `open_file`'s unknown-type path, after csv and json.
Svg and flat ODF have no signature of their own: `is_svg_file` and
`is_flat_opendocument_file` inspect the tree `XmlFile` already built, and the
more specific reading is reported next to plain xml. What is left over is a
source view: `.xhtml`, `.rels`, `.plist`, rss feeds.
