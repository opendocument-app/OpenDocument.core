# AGENTS.md — `internal/svg`

Read the root [`AGENTS.md`](../../../../AGENTS.md) first. This file covers what
svg does differently, and why.

## An svg is xml

`SvgFile` is an `abstract::ImageFile` over a `std::shared_ptr<xml::XmlFile>`.
The [xml module](../xml/AGENTS.md) parses, rejects what is not well formed, and
resolves the encoding from the declaration. What is left is one question — is
the root element `svg`? — answered against `XmlFile::root_name()`, which the
xml parse already recorded, so detection costs one parse and not two.

pugixml does not process namespaces, so the root name arrives with whatever
prefix the document bound (`<s:svg>`) and the prefix comes off by hand. Same
for attributes, matched on their local name.

`FileType::scalable_vector_graphics` is therefore no longer a label the generic
`common::ImageFile` will put on any bytes: `open` as an svg throws `NoSvgFile`
unless it is one.

## It renders as markup

Every other image goes into the page as `<img src="data:…">`. An svg is written
into the page as the markup it is, so it scales to the viewport and its text is
selectable.

An svg **inside** a document keeps the data url — `translate_image_src` — where
it is one image in a layout and `<img>` renders svg in secure static mode. A
starview metafile converted to svg (`html/image_file.cpp`) takes that path too.

## Which is why there is a sanitiser

Inside an `<img>` an svg is inert. In the page it is live markup, and the file
came from wherever the user got it. Two lines of defence:

1. **`svg::sanitize` scrubs the tree** — script, `foreignObject` and the other
   embedding elements, every `on*` attribute, every `href`/`src` that is not a
   same-document fragment or a `data:image/` url, SMIL animation aimed at any
   of those, and css that reaches outside the document (`@import`, or a `url()`
   that is neither a fragment nor a `data:image/`).
2. **The page declares `script-src 'none'`** — free, since no view of an svg
   has a script of its own, and it catches whatever the scrub missed.

Two rules need their reasons on record:

- **Everything is matched case-insensitively.** In html *foreign content* the
  parser lowercases names, so `<SCRIPT>` — a distinct element in
  case-sensitive xml — arrives in the page as an svg script and runs.
- **External references are dropped even though they cannot execute.** A
  `<image href="https://…">` or a css `url(https://…)` tells someone the file
  was opened, and no rendering path here has ever made a network request. That
  costs `<a href="https://…">` its href too.

DTDs are not processed and entities are not expanded — pugixml's behaviour,
which closes XXE and entity expansion by construction.

## The tree is not the bytes

The output is pugixml's `print` of the scrubbed tree, so original indentation,
attribute quote style and entity spelling are gone. Same trade as the xml
source view.
