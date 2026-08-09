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

- An xml file opens as xml and reads as a foldable, highlighted source view
  rather than as one very long line, in the encoding its declaration names.

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
