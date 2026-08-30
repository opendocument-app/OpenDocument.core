# AGENTS.md — `internal/svg`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file covers what
svg does differently, and why.

## An svg is xml

`SvgFile` is an `abstract::ImageFile` over a `std::shared_ptr<xml::XmlFile>`.
The [xml module](../xml/AGENTS.md) parses, rejects what is not well formed, and
resolves the encoding from the declaration. What is left is one question — is
the root element `svg`? — answered by `is_svg_file` against
`XmlFile::root_name()`, which the xml parse already recorded, so detection costs
one parse and not two.

pugixml does not process namespaces, so the root name arrives with whatever
prefix the document bound (`<s:svg>`) and the prefix comes off by hand.

`FileType::scalable_vector_graphics` is therefore no longer a label the generic
`common::ImageFile` will put on any bytes: `open` as an svg throws `NoSvgFile`
unless it is one.

`is_svg_file` is a predicate rather than a throwing check because
`open_strategy` asks the question without wanting the file: an xml that is not
an svg is handed on as the `XmlFile` already built, so the parse is not
repeated.

Every layer stays reachable from the one above — `xml_file()`, `text_file()`,
`file()`, plus `document()` and `text()` forwarded from the xml layer. A
downstream reader needs neither a second parse nor a second decode. Note that
`text_file()->text()` decodes with the encoding *detected over the bytes* while
`text()` uses the one the *declaration* names; for an xml document the latter is
the right answer.

## It renders as an image, like every other image

The markup goes into the page as `<img src="data:image/svg+xml;base64,…">` —
`html/image_file.cpp`, the same path as png and jpeg, and the same path an svg
*inside* a document takes through `translate_image_src`. Nothing in this module
renders.

That is a deliberate choice and worth keeping on record, because the obvious
improvement — inline the markup so the drawing scales to the viewport and its
text is selectable — costs more than it looks:

- **Inside an `<img>` a browser renders svg in secure static mode.** Scripts do
  not run, external references are not fetched, animation is frozen. That is a
  browser guarantee, free, and it holds for a file that came from wherever the
  user got it.
- **Inlined, the markup is live**, and it is the only path in the library where
  the input file authors the output DOM. Everywhere else we interpret the file
  and emit our own markup — text goes through `escape_text`, images become an
  `<img>` we construct. Inlining means `<script>` inside the svg *is* a script
  tag, `onload=` fires, `<image href="https://…">` fetches, `<foreignObject>`
  carries arbitrary html. Our output is displayed in a WebView with a bridge to
  native (`docs/design/editing.md`).
- So inlining requires **re-implementing secure static mode by hand** — a scrub
  of script and embedding elements, event handlers, references that leave the
  document, SMIL aimed at any of those, and css that reaches outside — plus a
  `script-src 'none'` policy on the page, plus stripping the prefix a document
  bound to the svg namespace (the html parser enters foreign content on `svg`,
  not on `s:svg`). It was written once and removed again; `git log` for
  `svg_util.cpp` has it, tests included.
- And a scrub **interferes with the file**: valid, harmless things go — an
  `<image href="chart.png">`, a webfont, half of SMIL. Someone who opens an
  animated svg gets a still.

If scalable, selectable svg is wanted later, isolation beats modification:
serve the file as its own resource in a sandboxed iframe, where the browser
contains it and the file stays intact.

## Writing svg is this module's other half

`svg_writer.*` is the counterpart to `html/html_writer.*`: elements,
attributes, style declarations and text, escaped, for code that *generates*
svg. Today that is `svm/svm_to_svg.cpp`, translating a StarView metafile.

Two things it does that a raw `operator<<` does not:

- **It escapes.** svg is xml, so an unescaped `&` in a chart label does not
  spoil one label, it costs the whole image — an xml parse error renders
  nothing at all. `html::escape_text` is the wrong tool for it: that one emits
  `&nbsp;`, which is an html entity and undefined in xml.
- **It formats numbers itself**, in fixed notation via `std::to_chars`,
  independent of whatever locale or precision the stream carries. A stream
  imbued with a german locale would otherwise write `1,5` into a coordinate,
  and a `style` declaration is css, where `1.2e+3` is not a length.

## The xml layer is not free

`XmlFile` holds the parsed tree for as long as the file is open, and pugixml's
dom is roughly twice the source. An svg costs that even though nothing reads
the tree after the root-name check — the price of detecting by reading rather
than by extension.
