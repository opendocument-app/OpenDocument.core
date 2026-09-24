# XML plan

Open work only. What is built is in [`AGENTS.md`](AGENTS.md).

## Test data

`FileTypeCapabilities.declaration_matches_the_engines` (`odr_test.cpp`) opens
files per type from the test data, and there are no `.xml` samples. Wanted: a
minified `content.xml` from an odt, a hand-formatted document with comments
and a doctype, one non-UTF-8 declared encoding, and one malformed file.
Everything a string literal can express stays inline in
`test/src/internal/xml/xml_file_test.cpp`.

## Size

A `content.xml` is routinely tens of megabytes. pugixml's dom is about 1.5 to
2 times the file, `XmlFile` holds it for its lifetime, and a span per token
can be 5 to 10 times the input in html. Both land in a WebView on a phone.

- A node budget in `HtmlConfig`, following `spreadsheet_limit`:
  `std::optional<std::uint32_t> xml_node_limit`, `nullopt` for unlimited.
  Past it, a visible truncation notice.
- Past a lower threshold, default the fold state to closed below some depth.
- Measure on a real `content.xml` before choosing the numbers.

## The archive seam

The filesystem view links every entry as an `application/octet-stream` data
url (`html/filesystem.cpp`), so browsing into a zip and opening
`word/document.xml` downloads it. Routing entries through `html::translate`
is a separate feature with its own questions (which types, resource paths,
how deep). Not scoped here.

## Deferred, by decision

- Byte-faithful mode: a lexer over the raw text that keeps the author's
  formatting and renders a malformed file up to the break.
- XSLT: an `<?xml-stylesheet?>` PI is shown as the processing instruction it
  is.
- Rendering xhtml as html. This is a source viewer.
- Namespace resolution. pugixml does not process namespaces, and a source view
  shows the prefixes the file uses.
- Expand-all, collapse-all and search within the tree. Each needs JavaScript
  of its own.
