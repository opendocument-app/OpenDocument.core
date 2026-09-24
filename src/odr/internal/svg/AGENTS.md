# AGENTS.md — `internal/svg`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file covers what
svg does differently, and why.

## An svg is xml

`SvgFile` is an `abstract::ImageFile` over a `std::shared_ptr<xml::XmlFile>`.
The [xml module](../xml/AGENTS.md) parses the file, rejects what is not well
formed, and resolves the encoding. `is_svg_file` then asks one question: is
the root element `svg`? It reads `XmlFile::root_name()`, so detection costs
one parse. pugixml does not process namespaces, so the prefix of `<s:svg>`
comes off by hand.

`SvgFile`'s constructor throws `NoSvgFile` if the root is not `svg`.
`is_svg_file` is a predicate, not a throwing check, because `open_strategy`
asks the question and hands a non-svg on as the `XmlFile` it already built.

Every layer stays reachable: `xml_file()`, `text_file()`, `file()`,
`document()` and `text()`. `text_file()->text()` decodes with the encoding
detected over the bytes. `text()` uses the encoding the declaration names,
which is the right one for an xml document.

`XmlFile` keeps the parsed tree while the file is open, and the pugixml dom is
about twice the source. That is the price of detection by content.

## It renders as an image, like every other image

The markup goes into the page as `<img src="data:image/svg+xml;base64,…">`
through `html/image_file.cpp`, the same path as png and jpeg, and the same
path an svg inside a document takes through `translate_image_src`. Nothing in
this module renders.

Do not inline the markup, even though inlining would make the drawing scale
and its text selectable:

- Inside an `<img>` the browser renders svg in secure static mode. Scripts do
  not run, external references are not fetched, animation is frozen.
- Inlined, the input file authors the output DOM. `<script>` runs, `onload=`
  fires, `<image href="https://…">` fetches, `<foreignObject>` carries html.
  The page runs in a WebView with a bridge to native.
- A scrub that rebuilds secure static mode by hand also removes harmless
  content, such as an `<image href="chart.png">`, a webfont or an animation.

If a scalable, selectable svg is wanted later, serve the file as its own
resource in a sandboxed iframe.

## Writing svg is this module's other half

`svg_writer.*` is the counterpart of `html/html_writer.*` for code that
generates svg. Today that is `svm/svm_to_svg.cpp`. It does two things a raw
`operator<<` does not:

- It escapes attributes through `xml::escape_attribute` and text through
  `xml::escape_text`. svg is xml, so one unescaped `&` in a label costs the
  whole image. `html::escape_text` is the wrong tool, because it emits
  `&nbsp;`, which xml does not define.
- It formats numbers through `util::number::to_string_significant`, never
  through the stream, because the stream carries the host locale.
  `format_number` writes `0` for a value that is not finite, because `nan` in
  an attribute drops the element.
