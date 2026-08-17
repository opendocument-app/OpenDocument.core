# Changelog

What changed in the engine, for whoever builds against it — the apps, the
bindings, and the packages on conan, maven, PyPI, npm and SwiftPM. Each
[release](https://github.com/opendocument-app/OpenDocument.core/releases) puts
its section on top of the generated per-pull-request list. Back to v5.3.0;
before that the releases are all there is.

An entry goes under `Unreleased` in the pull request that makes the change, and
only for what a consumer notices: API, formats, rendering, behaviour, packaging.
Refactoring, tests and CI are in the generated list already. A breaking change
says **Breaking** first.

The release run heads these entries with the version and opens a fresh
`## Unreleased`. A release with nothing under it is refused.

## Unreleased

- Prose is no longer read as a csv. A separator that every field follows with a
  space, in fields long enough to be sentences, is punctuation; `a, b, c` with
  short values is still a csv. One record is a line, not a table.

## v6.7.1 - 2026-08-16

- A pdf whose fonts are not embedded sits where the file puts it: a recovered
  word break was both written into the run and spanned by its offset, so every
  word drifted another space right and a search hit landed on its neighbour.
- A pdf's bold and italic text is bold and italic — a non-embedded font is
  asked for by the name of that cut, not faked at the regular cut's widths.
- A pdf line that mixes fonts or sizes puts each run at its own baseline
  instead of against the metrics of whichever run opened the line. Subscripts
  and superscripts sit where the file puts them, and the text after a bullet
  no longer parts company with the highlight that selects it.
- A search hit or a selection running across words in a pdf paints the word
  gaps between them rather than leaving a sliver of white in each. A gap wider
  than the text is left alone: it is a column of white, not a space.

## v6.7.0 - 2026-08-16

- `odr.search()`, `searchNext()`, `searchPrevious()` and `resetSearch()` now
  come with every view that renders text — a pdf, a text file, an xml view, an
  archive listing — as the shipped `search.css` and `search.js`, which a host
  that links rather than embeds has to serve.
- A search hit in a pdf tints its glyphs instead of covering them; a pdf's
  glyph layer and the text view's line numbers are not searched.
- A keyword is found across the spans it happens to be written in — a pdf puts
  a word in each — and a space matches whichever kind the page carries. It
  still does not run across a line, a cell or a paragraph.
- A tap on a paged document reaches its text: a negative `z-index` painted the
  page behind its container, which took every tap. No caret, and no keyboard on
  ios.

## v6.6.0 - 2026-08-14

- A paragraph states its own font, so a run that names none of its own is read
  at that font instead of at nothing. Slides whose text was there but invisible
  now show it.
- An empty paragraph keeps the height its font implies, whether or not the file
  names a size for it, and copying across one yields a blank line.
- A sheet's cells read in the font the file names, falling back to the sheet's
  own only where it names none.
- An image stays inside its frame on a page read without the shipped stylesheet,
  rather than covering the whole page.
- A frame that names a side instead of an offset sits on that side, so a centred
  image in an odt is centred. The side it names is `GraphicStyle`'s new
  `horizontal_position`, carried by the python, java and objc bindings.
- A pdf that comments its content stream renders: `%` to the end of the line is
  white space wherever it stands, not the start of a token.
- Text that decodes to half a surrogate pair costs that character a replacement
  mark instead of the whole document.
- A pdf's text sits where the file puts it: a line was placed against the
  browser's default font rather than its own, which dropped small text by
  several points.
- A pdf's cmyk colours are converted as Adobe converts them, so a process cyan
  reads as one instead of as pure `#00ffff`.
- A pdf page is the size of its crop box and shows what is on it and no more,
  as a viewer shows it.
- A pdf's JPEG 2000 images render. New dependency: `openjpeg`.
- A frame anchored to the page sits where the page says, whatever the text
  around it wraps like - a letter's address and date boxes land in their fields
  instead of in the running text.
- A text document is laid out on the master page it names, so a letter template
  keeps the margins that leave room for its letterhead.
- A docx or odt that chains its styles deeply opens instead of taking the
  process down with it: the `w:basedOn` / `style:parent-style-name` chain is
  walked onto a stack rather than recursed, so its length costs no stack.
- A document's xml parts are read once instead of buffered twice on the way into
  the parser, which lowers the memory opening a large one takes.

## v6.5.0 - 2026-08-10

- An xml file opens as xml and reads as a foldable, highlighted source view
  rather than as one very long line, in the encoding its declaration names.
- An svg is recognised by reading it rather than by what it is called, so bytes
  that are not one no longer open as an image that cannot be shown.
- A text file reads in a quieter gutter: the line numbers line up with their
  lines, stay out of a copy of the page, and the hovered line is marked.
- Every page states its own body margin and background instead of inheriting the
  browser's, so no view opens inset by an eight-pixel border.

## v6.4.0 - 2026-08-09

- csv opens as a spreadsheet, its dialect probed unless the caller gives one,
  and a value that only looks numeric left as text.
- Text encodings are named and decoded, so a document that is not UTF-8 comes
  out as text rather than mojibake.
- An archive's entries are served as files instead of one page of base64.
- A docx renders on the page it was written for, and a numbered list carries its
  markers as text a copy keeps.
- Sheets render in a quieter grid, under a ruler that stays put while scrolling.

## v6.3.0 - 2026-08-08

- The engine runs in the browser. A WebAssembly build published as
  `@opendocument/odr-core` renders documents with no server.
- A file can be opened from memory — `File::from_memory` — and every entry point
  that took a path now also takes a `File`.
- `translate` no longer needs a cache path. The overloads taking one remain and
  ignore it; nothing on the render path writes to disk.
- svg, ico, jxl, jp2, psd, wmf and emf are detected and translated as images.
- Embedded PDF fonts no longer render as tofu or as the wrong glyphs.
- Two translates can run at once. The unit registry and the logger clock were
  not thread-safe, and the HTTP server serves from a thread pool.
- argon2id is implemented in-tree, which drops the `conan-odr-index` remote:
  every dependency now comes from ConanCenter.

## v6.2.0 - 2026-08-02

- Objective-C bindings and a Swift package, with the `OdrCoreObjC.xcframework`
  attached to each release.
- The renderer's css and js are written into the html, so a consumer no longer
  ships `odr.js` or points the library at a data directory.
- Audio and video play in the browser, and the media formats the engine could
  only name are now opened.
- `mimetype` is answered without libmagic, and the libmagic option is
  deprecated.
- The html viewport config is bound for Java and Python.
- A release reports where it landed, so one that reached only some of the
  package registries is visible instead of silent.

## v6.1.0 - 2026-07-30

- The supported-format tables are public, with a per-file-type capability query,
  so a caller can ask what the engine does with a type before holding a file.
- The Java library and a new Android AAR are published to Maven Central, the
  AAR tested on a device.
- `HttpServer::stop()` and destruction wait for `listen()` to return.
- A JNI wrapper stays reachable while its handle is in a native call.

## v6.0.1 - 2026-07-27

- The logger thread attaches to the JVM on Android.

## v6.0.0 - 2026-07-27

**Breaking.** A cleanup of the public API, source-incompatible throughout.

- The deprecated API and the pdf2htmlEX / wvWare backends are gone, and with
  them `DecoderEngine` and the whole decoder-selection dimension — with the
  external backends dropped there was one engine left to select.
- `FileMeta` absorbs `DocumentMeta`, `Logger` is a value type with a public
  `ILogger` sink, and `HttpServer::listen` splits into `bind` and `listen`.
- Shape geometry is typed: `Frame`, `Rect`, `Line`, `Circle` and `CustomShape`
  return `Measure`, not `std::string`. `Measure::to_string` keeps 7 significant
  digits instead of 4, so drawing coordinates stop being rounded away.
- Every exception derives from `odr::Exception`; `Color` uses `from_rgb` /
  `from_argb`; `const char *` is gone from the public headers in favour of
  `std::string_view`.
- Two long-standing defects: `ElementIterator::operator++(int)` could not
  advance, and elements compared equal across unrelated documents.
- The python wheel ships the libmagic database inside it.

## v5.7.1 - 2026-07-26

- The Java API loads on Android API 26 again.
- Conan consumers can turn the cli off.

## v5.7.0 - 2026-07-26

- Python bindings (`pyodr`) live here rather than in `OpenDocument.py`.
- JNI bindings with an `app.opendocument.core` Java API, published to GitHub
  Packages.

## v5.6.0 - 2026-07-25

- Initial zoom is configurable through `HtmlViewportMode`, and fixed-size
  content — PDF and images — opens fit-to-width on mobile.

## v5.5.0 - 2026-07-25

- Legacy Microsoft formats gain formatting: character formatting in doc and ppt,
  cell fonts and fills in xls, and pictures, slide size and slide names in ppt.
- OOXML gains pptx slide size and tables, xlsx merged cells and value types, and
  docx table merges; ODF gains sub/superscript, percent line-height and
  first-line indent.
- PDF exposes per-page html views, and a page range limits any document.

## v5.4.1 - 2026-07-11

- **Relicensed from GPL-3.0 to MPL-2.0.**
- The HTTP server is optional (`ODR_WITH_HTTP_SERVER`), and asset bundling
  defaults to off.

## v5.4.0 - 2026-07-05

- **PDF is rendered by this engine.** Text is placed by its baseline, embedded
  TrueType, CFF, Type1 and Type3 fonts are used and missing ones substituted
  against standard-14 metrics, and images, paths, clipping, shadings, tiling
  patterns, transparency and blend modes are drawn. Links become `<a>` overlays.
- Encrypted PDFs open with the standard security handler, damaged
  cross-reference tables are recovered, and xref streams, object streams and
  hybrid files are read.
- Text extraction handles ToUnicode CMaps, composite fonts and the legacy CJK
  CMaps, so older documents no longer come out garbled.
- Initial ppt and xls support, read in-tree.

## v5.3.0 - 2026-06-06

Mostly build and CI.

- Plain-text output honours `HtmlConfig::editable`.
- CMake options are prefixed `ODR_`.
