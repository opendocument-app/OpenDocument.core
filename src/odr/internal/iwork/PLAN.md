# iWork plan

Where an iwork module goes, and in what order. Written before stage 1; kept
honest as stages land. **Stages 1, 2 and 5 have landed** — see
[`AGENTS.md`](AGENTS.md) for what they decided. Stage 5 was pulled ahead of 3
and 4 because it needed neither: a slide is a container above the text storage
stage 2 already read. Stage 6 is next.

## Today

A `.pages` opens as a text document and renders its body text; a `.key` opens
as a presentation and renders each slide's text boxes as positioned frames.
`.numbers` has a `FileType` entry and a `file_type_table.cpp` row so a caller
can name it, but no capabilities and no engine behind it.

Six fixtures are committed:
`test/data/input/odr-public/pages/{empty.pages,style-various-1.pages}`, written
by iWork 13.2, and `test/data/input/odr-public/{key/{empty.key,
style-various-1.key},numbers/{empty.numbers,style-various-1.numbers}}`, written
by iWork 14.4 (`Metadata/BuildVersionHistory.plist`). None is listed in
`index.csv` — they do not need to be, `TestData` picks up anything the file
type table knows an extension for — and each gained reference output when its
format turned `translate_html` on. Nothing decodes the `.numbers` pair yet;
they are what pins the Keynote-versus-Numbers detection rule.
`style-various-1.pages` carries `Index/Tables/` and nine files under `Data/`,
and `style-various-1.key` a table on its last slide, which is most of the
surface below.

## Spec

There is none, and none is coming. Nothing is vendored under
`offline/documentation/`, and Apple has never published the `.proto` schemas.

The evidence available is, in order of trust:

1. **The fixtures.** Byte layout verified against a file in the repo is the only
   claim this plan treats as fact; everything else below is marked as needing
   confirmation.
2. **MIT-licensed reverse engineering** — `numbers-parser`, `keynote-parser`,
   `obriensp/iWorkFileFormat`.
3. **`libetonyek`** (Document Liberation Project, MPL-2.0).

Read all of them for facts; **copy code from none of them**. Where `oldms/`
cites `[MS-XLS] §2.4.1`, this module cites a fixture and an offset instead —
`empty.pages Index/Document.iwa +0` — because that is the strongest reference
that exists.

## Target

`.pages` opens as a text document, `.numbers` as a spreadsheet, `.key` as a
presentation, all through the abstract document model, so the generic HTML
renderer and every binding get them without format-specific code.
`is_editable()` false, `save` throws, exactly as `oldms/text`.

**iWork '13 and later only.** The 2005–2009 XML era is a different format and is
deferred by decision — see below.

## Decisions taken up front

**The module is `iwork`, not `apple`.** `apple/` at the repo root is already the
Objective-C bindings and the Swift package; a second meaning would be a trap.
Namespace `odr::internal::iwork`.

**Three file types, one engine.** `iwork_pages`, `iwork_numbers`,
`iwork_keynote` appended to `FileType`, each with its own
`file_type_table.cpp` row and `DocumentType`. One `IworkFile` switches on the
type, exactly as `odf::OpenDocumentFile` (`odf_file.hpp:21`) does for the four
opendocument types — same constructor over an
`abstract::ReadableFilesystem`, same `document()` dispatch.

**No new dependencies.** Two pieces would normally be a conan line each, and
both are wrong here:

- **Snappy.** The `.iwa` framing is Apple's own: a 4-byte header per block,
  `0x00` then a little-endian 24-bit compressed length, repeated to EOF. Stock
  Snappy *framing* (the `sNaPpY` stream identifier, per-chunk CRC-32C) is not
  present, so a stream decoder does not apply — only the **block** decoder does,
  and that is a varint length plus literal/copy tags in about 150 lines.
  Verified: `empty.pages Index/Document.iwa` opens `00 fb 1d 00`, i.e. 7675
  bytes, and the file is 7679; the block then starts `82 6e`, a varint
  uncompressed length of 14082.
- **Protobuf.** Only the wire format is needed — varint, 64-bit,
  length-delimited, 32-bit, with groups skipped. There are no schemas to
  generate from, so a code generator would have nothing to do, and linking
  conan `protobuf` would drag one into the wasm, android and apple builds to
  replace ~150 lines. Read fields by number against hand-written accessors.

Both live inside `iwork/` for now. The csv plan's rule applies — anything
genuinely shared moves out to its own package *first* — and a wire reader with
one user has not earned a package.

**An `.iwa` is an object graph, not a tree.** Each file is a sequence of
`(varint length, TSP.ArchiveInfo, payload bytes)`. `ArchiveInfo` is field 1 =
identifier, field 2 = repeated `MessageInfo{1: type, 2: version, 3: length,
4: field_infos}`. Verified on the same fixture: `08 01` (identifier 1),
`12 52` (an 82-byte `MessageInfo`), `08 90 4e` (type 10000), `12 03 01 00 05`
(version 1.0.5), `18 e0 0c` (payload length 1632).

Objects reference each other by identifier, so the engine **indexes first and
resolves lazily**: build `identifier → (type, component, span)` across the
package, then walk the graph from the document archive. Reading files in
directory order and hoping the tree falls out is the mistake to avoid.

**Components come from `Index/Metadata.iwa`, not from file names.** It carries
the package's component list (identifier → locator). The fixture's names carry
id suffixes — `CalculationEngine-1732585.iwa`,
`AnnotationAuthorStorage-1732584.iwa`, `Index/Tables/DataList-1732820-2.iwa` —
so globbing for `CalculationEngine.iwa` finds nothing in one fixture and finds
it in the other. (`style-various-1.pages` has the unsuffixed spelling.)

**A pinned type table, and fail soft on what is not in it.** A `constexpr`
table maps message type IDs to the archive kinds we understand, annotated with
the app versions it was confirmed against, keyed off
`Metadata/BuildVersionHistory.plist`. An unknown type ID is **skipped**.

This is not a hole in the root `AGENTS.md`'s fail-fast rule, for the same
reason the rtf plan's leniency is not: that rule says throw *where the spec
dictates what to expect*. Here there is no spec, and an unknown type ID means
Apple shipped a version we have not mapped — not that the input is corrupt. A
reader that throws on one cannot open next year's files.

Do throw on: framing that overruns the file, a Snappy block that decompresses
past its declared length, a varint that does not terminate, a reference to an
identifier the index does not hold, and nesting past a depth bound.

**`Data/` needs no decoder.** Media is stored as ordinary zip entries; open it
through the filesystem, hand it to `ImageAdapter::image_file` with
`image_is_internal() == true`.

**Flat `ElementRegistry`, copied not shared.** An iWork document's element count
is bounded by its paragraph and drawable count, so the pattern in the root
`AGENTS.md` applies unchanged and `oldms/text/doc_element_registry.*` is the
template — not csv, whose id-is-the-coordinate scheme exists because a sheet has
an element per cell.

**Pages has two document modes.** Word processing is a text flow; page layout is
a canvas of floating text boxes, structurally closer to `odg` than `odt`. One
`FileType` and `DocumentType::text` either way — word processing first, page
layout rendered as pages of frames once frames exist (stage 4).

**A Numbers sheet holds many tables.** Our `Sheet` (`document_element.hpp:300`)
is one grid plus `shapes()`. Map **one odr sheet per Numbers table**, named
`<sheet> – <table>`; taking only the first table per sheet would drop data
silently.

**No writer, no editing.** Out of scope in every stage.

---

## Stage 1 — detection and the container *(landed)*

Nothing renders yet. The point is that the bytes come apart correctly and the
type is reported, which is also the whole of what a file picker needs.

Landed as planned, with three deviations:

- **Only `iwork_pages` detects and opens.** `iwork_numbers` and `iwork_keynote`
  are classification-only rows, because the root archive type of a `.numbers`
  or a `.key` cannot be read off a fixture that does not exist, and this module
  does not guess.
- **`password_encrypted()` is not answered.** `Index/Metadata.iwph` was going
  to report it, but nothing here has ever seen an encrypted package; an
  encrypted one is one whose `Index/Document.iwa` does not decompress, and it
  falls back to being reported as a zip.
- **Detection does not read `Index/Metadata.iwa`.** It reads
  `/Index/Document.iwa` straight, since it runs on every zip a caller opens and
  `Document` is the one component whose file name never carries a suffix. The
  component list is read when the document is.

- `iwork_snappy.{hpp,cpp}` — Apple framing plus block decompression, over the
  `std::istream *` / `std::streambuf *` shape `pdf::ObjectParser` uses
  (`pdf_object_parser.hpp`).
- `iwork_protobuf.{hpp,cpp}` — wire reader: varints, the four wire types, skip
  for unknown fields, a `NestingGuard` for length-delimited recursion.
- `iwork_archive.{hpp,cpp}` — `.iwa` → objects; the package index built from
  `Index/Metadata.iwa`; `identifier → object` lookup.
- `iwork_file.{hpp,cpp}` — `IworkFile : abstract::DocumentFile` over an
  `abstract::ReadableFilesystem`, shaped like `odf::OpenDocumentFile`.
  `password_encrypted()` reports true when `Index/Metadata.iwph` is present and
  `decrypt` throws (see Deferred).
- detection: a third probe after odf and ooxml in `list_file_types`
  (`open_strategy.cpp:213-238`), a branch in `open_file_as` beside odf's
  (`:48-66`), and in `open_file` (`:328`) and `open_document_file` (`:498`).
  `Index/Document.iwa` present ⇒ iWork; **which app comes from the root
  archive's type ID**, read off a fixture per app and put in the pinned table —
  not guessed, and not taken from the extension, which a caller may have lost.
- three rows in `file_type_table.cpp`: extensions `pages` / `numbers` / `key`,
  mime types `application/vnd.apple.{pages,numbers,keynote}` plus the
  `application/x-iwork-*-sff*` spellings, `FileCategory::document`, the matching
  `DocumentType`, `{.detect_by_content = true, .open = true}`. `odr_test` fails
  if declared capabilities exceed what the engine does, so `translate_html`
  arrives per format in the stages below.
- `FileType` entries at the end of the enum; the three bindings above.
- `CMakeLists.txt` `ODR_SOURCE_FILES`.

**Tests** are inline byte strings: a hand-built Snappy block (literal, copy with
1-byte and 2-byte offsets, a length that overruns), varint edge cases
(continuation past 10 bytes, a 64-bit field), an `ArchiveInfo` with an unknown
type ID that must be skipped rather than thrown on, and framing truncated
mid-block. Only the type-reporting test needs the fixtures — the data repos are
fetched and optional, so everything that can be inline is.

## Stage 2 — Pages text *(landed)*

- walk from the document archive to the body's text storage
  (`TSWP.StorageArchive` in the reverse-engineering literature; confirm the type
  ID against the fixture before pinning it).
- the storage holds the text as a small number of large strings plus **parallel
  run tables** — index/value pairs for paragraph styles, character styles and
  attachments. **Paragraph boundaries come from the paragraph run table**, not
  from splitting on `\n`; `U+2028` is a line break inside a paragraph and maps
  to `line_break`. The text is already UTF-8, so `internal/encoding` is not
  involved.
- `root → paragraph → span → text` in a flat `ElementRegistry`.
- `empty.pages` is the regression that matters here: an empty document must
  produce an empty body and not an exception.
- table row: `iwork_pages` gains `.translate_html = true`.

Landed as planned. What the fixtures settled: the body storage is field 4 of
`TP.DocumentArchive` (type 10000) and is a `TSWP.StorageArchive` (type 2001);
its paragraph style table is field 5, a `TSWP.ObjectAttributeTable` whose
field 1 repeats the entries — so the run tables are one level deeper than
"repeated entries on the storage". Run-table indices count UTF-16 code units
against UTF-8 text, which the parser translates in one pass.

## Stage 3 — Pages styles

- `Index/DocumentStylesheet.iwa`. Style archives are sparse property sets with a
  parent reference, so resolution is an inheritance walk terminating at the
  theme's default — cache resolved styles by identifier, and let equal property
  sets share one resolved style the way `.doc` does.
- character and paragraph properties → `TextStyle` / `ParagraphStyle`; adjacent
  equal-style runs merge into one span.
- font names interned in the style registry and never mutated afterwards, so
  `TextStyle::font_name` (`const char *`) stays valid — the `.doc`/`.xls` rule.
- page geometry → `TextRootAdapter::text_root_page_layout`, which stage 2
  returns empty from.

## Stage 4 — drawables, images, frames

- drawable archives carry a geometry (position, size, transform) and a content
  reference → `Frame` plus `Image`, `Rect`, `Line`, `CustomShape` as they map.
- images resolve to a `Data/` entry by name; hand the zip entry through
  unchanged.
- Pages **page-layout mode** falls out here: it is drawables on pages with no
  body flow, so once frames exist it is a different root assembly, not new
  parsing.

## Stage 5 — Keynote *(landed)*

Cheaper than it looks, and therefore before Numbers: slides are a container
above the *same* text storage stage 2 and 3 already read.

- slide archives → `Slide`, the slide's master → `MasterPage`, text boxes and
  drawables reuse stages 2–4 unchanged.
- notes, builds and transitions are out (see Deferred).
- `iwork_keynote` gains `.translate_html = true`; needs a `.key` fixture in the
  public data repo first.

Landed with three deviations:

- **The root archive does not say which app wrote the package.** The plan
  assumed it did, as it does for Pages. `KN.DocumentArchive` and
  `TN.DocumentArchive` are both type 1 — ids are namespaced per app — so
  Keynote is told from Numbers by the `Slide` components a deck holds. See
  `AGENTS.md`.
- **Masters are not read.** `slide_master_page` is null; the
  `Index/TemplateSlide-*.iwa` components are there and unopened, which costs
  the theme background and nothing a reader misses in text.
- **Frames arrived here rather than in stage 4.** A slide is drawables on a
  canvas, so there was no "text and nothing else" shape to land first. The
  geometry the drawable already carries is read, which is the piece stage 4
  needs for Pages; what stage 4 still owes is images, shapes and the anchoring
  a Pages text flow does.

## Stage 6 — the tile reader

Tables in iWork are stored as **tiles** — row ranges holding packed cell
records — with strings, formats and formulas kept in side "data lists" that
cells reference by index. Several cell-storage layouts exist across app
versions; this is where the pinned table earns itself, and where a version we
have not mapped must degrade to an empty cell rather than a wrong one.

Do this for **Pages tables first** (`Table`, `TableRow`, `TableCell`), because
`style-various-1.pages` already carries `Index/Tables/` and exercises the reader
without any Numbers archive being mapped.

## Stage 7 — Numbers

- sheets → one odr `Sheet` per table (see decisions), on top of stage 6.
- cached values only. `CalculationEngine.iwa` holds the formula graph and is not
  read; a cell shows what Numbers last computed, which is the position `.xls`
  takes.
- `iwork_numbers` gains `.translate_html = true`.

---

## Deferred, by decision

- **iWork '05–'09** (Pages 1–4, Keynote 1–5, Numbers 1–2) — gzipped XML,
  `index.xml.gz` for Pages/Numbers and `index.apxl(.gz)` for Keynote. A
  different format needing a different engine and its own stage 1, for files no
  application has written since 2013. If it ever happens it lives at
  `iwork/legacy/` behind the same three `FileType`s, which is why those are
  named for the app and not the era.
- **Password-protected files** — `Index/Metadata.iwph` and encrypted `.iwa`s.
  `password_encrypted()` reports it from stage 1; `decrypt` throws.
- **The package form.** On macOS a `.pages` can be a directory rather than a
  zip. The engine would not notice — `common::SystemFilesystem`
  (`common/filesystem.hpp:16`) roots at a directory and satisfies the same
  interface — but nothing in the public API opens a directory today, so this is
  a seam, not a stage.
- **Templates and iBooks Author** (`.template`, `.kth`, `.nmbtemplate`, `.iba`)
  — same container; add the extensions to the existing rows once documents
  work.
- **Charts, comments, footnotes, change tracking, formulas, `ViewState.iwa`,
  Keynote builds and presenter notes.**
- **Writing and editing.**
- **A preview fallback.** Every iWork file embeds a rendered preview
  (`preview.jpg`, `preview-web.jpg`, `preview-micro.jpg` in the fixtures; the
  '09 era wrote `QuickLook/Preview.pdf`). Deliberately not an iWork feature:
  odf and ooxml carry thumbnails too, so rendering a preview in place of a
  decode is one cross-cutting mechanism with one honest capability story, to be
  decided library-wide — not invented here, where it would make
  `translate_html = true` claim a fidelity this engine does not have.

## Test data

`empty.pages` and `style-various-1.pages` are in
`test/data/input/odr-public/pages/`. They need no `index.csv` row —
`TestData::test_files` picks up any file whose extension the file type table
knows — and reference output was regenerated when stage 2 flipped
`translate_html` on.

`empty.numbers` and `style-various-1.numbers` are in
`test/data/input/odr-public/numbers/`. Stage 7 is what will decode them; today
they are the negative that pins detection
(`IworkKeynote.a_numbers_package_is_not_keynote`). Everything at container
level stays inline, per stage 1.

The `.key` fixtures were authored on macOS with Keynote 14.4 rather than found:
there is no spec, so a file the app wrote is the only citation available, and
one written to order can carry exactly the shapes a stage needs — a title
slide, a bulleted body, an empty placeholder, a free text box and a table.
