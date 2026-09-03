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

- A `.docx` `wp:anchor` drawing floats at its offset with its wrap and side,
  instead of sitting in the line. Closes #803.

- A `.docx` table's own borders (`w:tblPr/w:tblBorders`) are drawn, and no
  longer four times too thick. New `TableStyle::border`,
  `::border_inside_horizontal` and `::border_inside_vertical`, mirrored in the
  JNI, Apple and Python bindings.

- A table cell in a text document starts its content at the top, as word and
  odf do.

- A fitted or zoomed view scales its text with the page in WebKit, where the
  type used to stay at its unscaled size. `HtmlViewportMode::fit_width_by_view`
  is usable on iOS. Closes #761.

- A PDF `gs` applies the `/ExtGState` stroke parameters `/LW`, `/LC`, `/LJ`,
  `/ML` and `/D`. A producer that sets the line width only there — Canva does —
  used to have every stroke drawn at the initial width of 1.

- A manual page break starts a new page box, and prints as `break-before:page`.
  New `BreakType` and `ParagraphStyle::break_before`/`break_after`, mirrored in
  the JNI, Apple and Python bindings. Towards #174.

- ODF's `text:soft-page-break` is no longer part of the element tree, so a
  `DocumentPath` past one shifts.

- New `DocumentFile::thumbnail()`, the preview the package carries or
  `nullopt`, mirrored in the JNI, Apple and Python bindings. Closes #21.

- **Breaking** (Swift only): `HtmlConfig`'s optional settings are Swift
  optionals of the real type rather than `NSNumber`/`NSValue` boxes —
  `spreadsheetLimit` is a `TableDimensions?`, `initialZoom` a `Double?`, and so
  on. ObjC callers see the boxed properties unchanged. Closes #759.

- The HTML renderer warns, rather than silently dropping, when it reaches an
  element type it has no `translate_*` for. Across the test corpus that is
  `page_break` and nothing else. Towards #150.

- An ODF `draw:object` chart is drawn from the chart part's own markup, not
  from the replacement image beside it: bar, line, area, scatter, pie and ring,
  with their titles, legends, axes and series colours. An object holding no
  chart we can read keeps the replacement. Closes #179.

- An ODF custom shape is drawn as the shape its `draw:enhanced-geometry`
  describes rather than as its bounding box: `draw:enhanced-path`,
  `draw:equation`, `draw:modifiers` and the two mirror attributes. Closes #159.

- ODF draws the shape elements it used to drop whole: `draw:path`,
  `draw:polygon`, `draw:polyline`, `draw:regular-polygon`, `draw:connector`,
  `draw:ellipse`, `draw:measure` and `draw:caption`. New `path()` on
  `CustomShape`, mirrored in the JNI, Apple and Python bindings.

- An ODF `draw:circle` is drawn as an ellipse rather than a circle inscribed in
  its box.

- An ODF drawing shape is drawn where its `draw:transform` puts it. New
  `transform()` on `Frame`, `Rect`, `Line`, `Circle` and `CustomShape`,
  mirrored in the JNI, Apple and Python bindings.

- A StarView metafile's text is decoded by the charset it names. Every
  encoding but `UCS2` used to emit the file's own bytes, and the invalid utf-8
  a non-ascii label made of that cost the whole image.

- A curve in a StarView metafile is drawn as a curve. The polygon flags that
  say which of its points are bezier control points were read and dropped, so
  every curve came out as a line through them.

- A StarView metafile's map mode is read in full: its unit, so a drawing that
  switches to twips or points is no longer off by the factor between them, and
  the relative map mode, which composes with the one before it rather than
  replacing it.

- A StarView metafile draws its gradients, hatches and transparent shapes:
  `GRADIENT`, `GRADIENTEX`, `HATCH` and `TRANSPARENT`.

- A spreadsheet decodes in less memory: 626 MB peak instead of 914 MB on a
  297 MB `content.xml`. Rendered output is unchanged.

- A StarView metafile draws its remaining primitives: `LINE`, `ELLIPSE`,
  `ROUNDRECT`, `ARC`, `PIE`, `CHORD`, `POINT` and `PIXEL`. The lines are what
  rules a table drawn as a metafile, which used to come out as floating
  numbers.

- A StarView metafile clips what it says it clips: `CLIPREGION` and the two
  that intersect one into it. A drawing that ran outside the clip used to be
  shown in full.

- A StarView metafile draws its bitmaps (#194): `BMP`, `BMPEX` and their
  scaling and part variants, transparency mask included. Such a chart used to
  render as an empty frame.

- A text action in a StarView metafile draws the run it names, not the whole
  string. Text drawn in runs used to overprint itself into a smear.

- StarView metafile text keeps its font: italic, bold, underline, strikeout,
  rotation, alignment, per-character positions, and the width a stretched run
  fills (#95).

- A StarView metafile draws through a graphics state stack: a colour, font or
  map mode set inside a `PUSH` no longer leaks past its `POP`.

- A filled StarView shape keeps its outline, a poly-polygon cuts its holes, a
  line takes its `LineInfo`, and the font size scales with the drawing.

- Text in a StarView metafile is escaped into the svg it renders as. An `&` or
  `<` in a label used to cost the whole image.

- A StarView metafile translation logs what it drops: unimplemented actions,
  and a failure it used to fall back from silently.

- An html attribute value drops the control characters xml forbids. One
  escaper writes both html and svg now.

## v6.12.0 - 2026-08-30

- New `Document::save(std::ostream &)` and `Document::save_to_memory()`, which
  returns the saved document as an in-memory `File`. The path overloads are
  unchanged.

- The wasm binding gained `edit(diff)`, `save()`, `isEditable()` and
  `isSavable()`, so a browser can save an edit back.

- Memory saves in the other bindings: `save_to_memory()` returns `bytes` in
  python, `saveToMemory()` `byte[]` in java, `-saveToMemoryWithError:` `NSData`
  in Objective-C.

- Two entries of the same zip document can be read at once; nothing serialises
  on a single lock any more.

- The zip backend is `miniz/3.1.1`, up from `3.0.2`. Rendered output is
  unchanged.

- New `FileType::hypertext_markup_language` for `html`, `htm`, `xhtml`,
  `text/html` and `application/xhtml+xml`. Classification only: no `open`, no
  `translate_html`, and never detected from its bytes.

- **Breaking** An ods sheet stores a repeated cell once, at the range it
  covers, rather than once per position: `SheetCell::position()` reports the
  anchor of that range instead of the position the cell was looked up at, and
  `Sheet::cell()` hands back the same element everywhere in it.
- An ods `table:number-columns-repeated` beside a `table:number-rows-repeated`
  no longer inflates — the grid's own `1048576 × 1024` asked for three billion
  elements out of four hundred bytes.
- An xlsx `<mergeCell>` whose `ref` names more positions than the sheet has
  cells opens again; `ref="A1:XFD1048576"` used to visit all 17 billion.
- Decoding a spreadsheet costs about half the memory it did — pugixml is built
  in compact mode and the ods sheet index is sorted vectors rather than maps.
  A consumer that includes an internal header now needs `PUGIXML_COMPACT`, and
  the conan package declares it.

## v6.11.0 - 2026-08-29

- No view declares `<base target="_blank">` any more. A link back into what
  serves the page — an archive entry, a PDF `#pN` anchor, a relative hyperlink —
  navigates in place; only a link that leaves the page carries
  `target="_blank" rel="noopener noreferrer"`.

- A document hyperlink renders without an `href` unless its target is `http`,
  `https`, `mailto`, `ftp`, `ftps`, `tel` or a relative reference — the
  allowlist a PDF `/URI` action already went through. `Link::href()` is
  unchanged.
- `HtmlView::sheet_cut` reports the extent a sheet's cells span against the
  extent the rendered markup carries, or nothing where the limits cut nothing.
  Bound in python, jni, wasm and apple as `sheet_cut` / `sheetCut`.
- New `HtmlConfig::spreadsheet_cell_limit`, 500000 cells for one sheet, bounds
  the rows a sheet keeps by its width; `spreadsheet_limit` rises from 10000 to
  100000 rows. wasm gains both.
- A saved document opens in LibreOffice again: every zip entry's size goes into
  its local header instead of a trailing data descriptor, which LibreOffice
  rejects on a stored entry — an odf package always stores `mimetype`.
- `HtmlConfig::min_content_margin` puts a floor under the distance the
  generated content keeps from the view's border, per side. A set side raises
  the inset a view already has, never lowers it; a sheet is never inset. Bound
  in python, jni, wasm and apple as `minContentMargin`.
- **Breaking** Bytes that do not read as text no longer come back as
  `text_file` and render as nonsense - `decode` throws `UnknownFileType` and
  `list_file_types` comes back empty. A file is text when it is empty, or its
  encoding can be named and it carries no NUL outside utf-16 and utf-32.
- `open(file, as)` no longer returns a container holding another document type
  as the type that was asked for. An encrypted ooxml still opens as whichever
  was asked - it names no inner type until it is decrypted.
- `.fodt`, `.fodp`, `.fods` and `.fodg` open, render, edit and save like their
  packaged counterparts instead of decoding as an xml source view. An
  `office:binary-data` image now decodes in a package too.
- `Document::as_filesystem` answers with an empty filesystem for a document
  that is one file rather than a package.
- A `.pages` file opens as a text document and renders its body text and the
  tables it anchors, instead of the zip it is made of; styles and images are not
  read yet.
- A `.key` file opens as a presentation and renders the text of each slide in
  boxes where the file positions them, instead of the zip it is made of; slide
  masters, styles, images, tables and presenter notes are not read yet.
- A `.numbers` file opens as a spreadsheet instead of the zip it is made of,
  with one sheet per Numbers table named `<sheet> – <table>`. Cells show the
  values the app last computed; number formats, merged ranges, charts and
  formulas are not read yet.
- An rtf opens and renders as a text document instead of throwing
  `UnknownFileType`. Its text, encoding, paragraphs and tabs are read;
  formatting, tables and pictures are not yet.
- A markdown file opens and renders as prose: CommonMark plus GFM, parsed with
  md4c (a new dependency); raw html, images and rules are not modelled yet.
  Like a csv it stays a text file that also loads as a document, through
  `as_markdown_file()`.
- Markdown has no signature, so it decodes only when `FileType::markdown` is
  asked for — a `.md` still comes back as a text file.
- `psd`, `jp2`, `wmf` and `emf` no longer declare `translate_html` — no browser
  paints them, and `html::translate` throws `UnsupportedFileType` instead of
  writing a blank page. They are still detected and still open.
- `odr.setZoom(value, focus)` holds the point the pinch is centred on. Webkit
  does not carry an applied `body{zoom}` in `getBoundingClientRect`, so the
  focus moved with the zoom.
- New `odr.getViewportRect(element)`: the element's box in the coordinates
  `elementFromPoint` takes, for a host hit-testing while a zoom is applied.
- A view whose zoom does not follow the viewport — `viewport_width`,
  `initial_zoom`, a sheet — keeps the reader's place across a width change.
- New `HtmlViewportMode::fit_width_by_view`: the view measures the fit and keeps
  it current, so a rotation refits instead of holding what it opened at. For a
  host whose web view does not fit a top-level document itself.
- A `.pptx` whose package relates a part that is not xml — a Google Slides
  export relates a protobuf — opens instead of throwing `NoXmlFile`. Only the
  slides `p:sldIdLst` names are read.
- A `.pptx` line break (`a:br`) renders as one instead of joining the runs
  around it, and a run's font, a paragraph's alignment and its left and right
  margins arrive: they were read from the wordprocessingml attributes, which a
  pptx never carries.
- New `PageLayout::background_color`, the ground a page is painted on. A `.pptx`
  slide takes it from the slide, its layout or its master; nothing else sets it
  yet.
- A `.pptx` renders in colour: text, highlights and shape fills, including the
  ones its theme names. Line height, space before and after, sub/superscript and
  vertical text alignment arrive with them. Tinted and shaded theme colours
  still come out at full strength, and shapes a master or layout draws are still
  missing.
- An odf shape or frame that is not filled no longer paints the colour of one
  that was.
- Text in a pdf can be selected, searched and copied where it came out as CJK
  before. A simple font's codes are one byte each, whatever codespace its
  `ToUnicode` map declares, and producers routinely declare two.
- A pdf page that is off screen is no longer laid out or painted. On a drawing-
  heavy file this halves the time to open and takes a zoom step from ~340ms to
  ~50ms.
- A pdf image placed on several pages is one image, and the pdf view honors
  `HtmlConfig::embed_images`, which it used to ignore.
- Text in a pdf no longer comes out in giant overlapping type on a browser with
  a minimum font size. A line block one would clamp is laid out at 24px and
  scaled back down. Such a run is also placed more accurately.

## v6.10.1 - 2026-08-21

- A linked image in a docx or xlsx (`embed_images = false`) is named relative
  to the document, like odf, so a host serving the html under a mount point
  resolves it.

## v6.10.0 - 2026-08-20

- Html views take a zoom from their host: `odr.getZoom()`, `setZoom(value,
  focus)`, `adjustZoom()`, `resetZoom()`, `isZoomFitted()`, `onZoomChange`.
  `HtmlConfig::initial_zoom` sets what they open at, script or no script.
- A pdf that nests parentheses inside a string opens, and keeps its document
  metadata — `cairo` and `pdfTeX` write their `/Producer` that way.
- A jni build without a JDK fails instead of shipping a package missing
  `odr-core-java.jar`. `ODR_JNI_JAR=OFF` is how the AAR build asks for the
  native half alone.
- A `FileWalker` over a document or an archive can be steered: `flat_next()`,
  `pop()` and `depth()` do what they say instead of nothing.
- A directory a document or an archive only implies is one: `exists()` and
  `is_directory()` answer for `/` and for a path that files sit under.
- `Odr.load()` works under a Content-Security-Policy without `'unsafe-eval'`:
  the wasm module is linked with `-sDYNAMIC_EXECUTION=0`, so embind builds its
  invokers without `new Function`. `script-src 'self' 'wasm-unsafe-eval'` is
  now enough.
- `wasm/README.md` says what Content-Security-Policy the rendered output needs,
  and what each directive is for. The failures are quiet: a pdf whose `data:`
  fonts are blocked renders as tofu rather than falling back.
- An odf table keeps the rows and columns a grouping element holds, such as the
  `<table:table-header-rows>` LibreOffice writes for a repeating header row.
  They were dropped from the output entirely, in text documents and
  spreadsheets alike.
- Text copied out of a pdf laid out glyph by glyph reads as words, not as
  `L o r e m`. A word break also survives a run with nothing extractable in it.
- An encrypted `.doc`, `.ppt` or `.xls` reports itself encrypted and raises
  `FileEncrypted` instead of a parse error, so a reader can prompt for the
  password. Decrypting them is still out of reach.
- Marking text in a rendered pdf highlights the words rather than a trail of
  narrow boxes beside them.
- A pdf that writes `Tm(text)Tj`, with nothing between the operator and the
  string, renders that text instead of dropping it.
- An embedded font that will not re-encode says so in the log rather than being
  swapped for a substitute in silence.
- Paged output rendered **into a frame** fits the viewport itself. A viewport
  meta tag is honoured for the top-level document only, so an embedder got no
  fit at all. Only ever down, and a top-level document is left to the meta tag.
- **New** `HtmlConfig::viewport_width`: the width the output will be shown at,
  in css pixels. The fit is then a factor in the emitted css — no script, framed
  or not. Bound in the python, wasm, jni and apple bindings as `viewportWidth`.
- An image view fits the viewport too: `img{max-width:100%}`, so a scan wider
  than the frame stops overflowing. `actual_size` still shows it 1:1.
- Output that fits itself keeps the reader's place when the viewport changes.
  The browser's own guess is wrong here because the scale changes with the
  width, and a long document came back a page or more from where it was.

## v6.9.0 - 2026-08-18

- A filled pdf form shows what was filled in, and a marked-up one its markup:
  an annotation's appearance stream is painted onto the page. Hidden, no-view
  and popup annotations stay unpainted.
- A scanned pdf page is no longer blank: `JBIG2Decode` images decode in house.
  MMR/Huffman, refinement and halftone regions still skip the image.
- Justified pdf text is spaced as the file asks: word spacing (`Tw`) applies to
  runs painted with an embedded font, which had rendered short.
- A pdf page is turned as its `/Rotate` says.
- A pdf whose subset font names its glyphs `gidNNNNN` reads correctly, and a
  named glyph variant such as `hyphen.case` is the one painted: a simple font's
  `/Encoding` names select through the font program's charset.
- A pdf whose Flate streams omit the ADLER32 trailer opens.
- A `.docx` is spaced the way word spaces it: paragraph spacing before and after
  and line height (`w:spacing`), the `w:contextualSpacing` that keeps a list
  tight, and table row heights (`w:trHeight`, as a minimum height).
- A `.docx` table follows its table style: the paragraph and text properties
  `w:tblStyle` names reach everything in the table.
- **Deprecated** `HtmlConfig::embed_outline`, `no_drm`,
  `background_image_format` and `background_image_dpi`. Nothing has read them
  for a while; they still store and return what is set.

## v6.8.0 - 2026-08-18

- A file can be read in the dark: `HtmlConfig::color_scheme` is `light`, `dark`
  or `system`, following the reader's `prefers-color-scheme`. Every view honors
  it but the pdf one, and the colors the file authored give way to it. Bound in
  the python, wasm, jni and apple bindings as `HtmlColorScheme`.
- `FileTypeCapabilities::color_scheme` says whether a type's view honors it —
  every type that renders but pdf, audio and video. Bound in the bindings too.
- A text document reflowed to the viewport is inset 3mm from the screen edge
  rather than starting at the first pixel of it.
- Prose is no longer read as a csv. A separator that every field follows with a
  space, in fields long enough to be sentences, is punctuation; `a, b, c` with
  short values is still a csv. One record is a line, not a table.
- A large text file appears at once: sizing the line numbers laid the page out
  once per line. A megabyte took 38s and now takes under a second.

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
