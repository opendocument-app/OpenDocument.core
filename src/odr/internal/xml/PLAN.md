# XML plan

What is left to build. The module as it stands is in [`AGENTS.md`](AGENTS.md).

## Owed — test data

`FileTypeCapabilities.declaration_matches_the_engines` (`odr_test.cpp`) opens
files per type from the test data, and there are no `.xml` samples for it to
open. Wanted: a minified `content.xml` lifted from an odt, a hand-formatted
document with comments and a doctype, one non-UTF-8 declared encoding, and one
file that is xml-shaped but malformed.

Everything a string literal can express stays inline in
`test/src/internal/xml/xml_file_test.cpp`.

## Size

A `content.xml` is routinely tens of megabytes, and this path multiplies it:
pugixml's dom is roughly 1.5–2× the file and `XmlFile` holds it for its
lifetime, and a span per token can be 5–10× the input in emitted html. Both
land in a WebView on a phone.

- a node budget in `HtmlConfig`, following `spreadsheet_limit` —
  `std::optional<std::uint32_t> xml_node_limit`, `nullopt` for unlimited — and
  past it a visible truncation notice rather than a silently short document.
- past a lower threshold, default the fold state to closed below some depth.
  The only case where collapsed-by-default is right.
- measure before choosing the numbers, on a real `content.xml`.

## The archive seam

The filesystem view links every entry as an `application/octet-stream` data url
(`html/filesystem.cpp`), so browsing into a zip and looking at
`word/document.xml` downloads it. Routing entries through `html::translate` is
a separate feature with its own questions (which types, resource paths, how
deep), and it is the one that turns this into a way to inspect any office
document's parts. Named so the dependency is on record; not scoped here.

## Deferred, by decision

- **Byte-faithful mode.** A lexer over the raw text, sharing the css, keeping
  the author's formatting and able to render a malformed file up to the point
  where it breaks. Not worth two renderers before there is one.
- **XSLT.** An `<?xml-stylesheet?>` PI is shown as the processing instruction
  it is. Applying it means an XSLT engine, larger than every format here put
  together.
- **Rendering xhtml as html.** This is a source viewer.
- **Namespace resolution.** pugixml does not process namespaces, and a source
  view should show the prefixes the file uses.
- **Expand-all / collapse-all** and **search within the tree** — both need
  JavaScript, and neither is worth being the reason this view ships a script.
- **Dark mode**, a question for `frontend.cpp` as a whole.
