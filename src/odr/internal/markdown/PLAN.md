# Markdown plan

The open work, in order. What the module does today is in
[`AGENTS.md`](AGENTS.md). CommonMark plus GFM (tables, strikethrough, task
lists, permissive autolinks) is the scope. Every other dialect stays out.

## Stage 4: images and frontmatter

- Images stay external. `ImageAdapter` offers `image_is_internal()` plus
  `image_href()`, so `![](diagram.svg)` becomes a `Frame` plus `Image` with
  `is_internal() == false` and the href passed through. No filesystem, no
  relative-path resolution, no fetching. The viewer resolves the href, because
  only it knows where the document came from.
- Watch the sizing. `html::translate_image` writes the `<img>` at
  `width:100%;height:100%` inside the frame's `div`, so a frame with no width
  and height renders nothing. Markdown carries no dimensions, so that path
  needs an answer before a frame is worth creating. Until then `MD_SPAN_IMG`
  is transparent and the alt text flows through.
- YAML or TOML frontmatter: md4c does not know it. Strip a leading `---` fence
  before parsing and expose the flat scalars (`title`, `author`, `date`)
  through `FileMeta`. Take no YAML dependency for the rest.

## Stage 5: two model gaps shared with odf

Both are api changes shared with the other engines, so they come last and
together.

- Heading level. `ElementType` has no heading, and odf maps `text:h` to a
  paragraph too. Either an `ElementType::heading` with a level, or a level on
  `ParagraphStyle`. The latter is smaller and lets the renderer emit `<h1>` to
  `<h6>` for odt and markdown at once. Decide with odf in the room.
- Code-block language. A string on the paragraph, or a dedicated code element.
  This is what lets a frontend syntax-highlight, and what makes a mermaid,
  graphviz or plantuml fence work: odr keeps the fence as a code block that
  carries its language, and the frontend renders it. A graph layout engine in
  C++ is a larger project than markdown support entire.

## Deferred

- Raw html passthrough. Inline SVG is blocked behind the same decision.
- Named entities beyond the XML five plus `&nbsp;` (`AGENTS.md`, known gaps).
- Footnotes, definition lists, `back_translate` to markdown.
