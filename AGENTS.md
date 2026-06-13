# AGENTS.md — OpenDocument.core

Orientation for AI agents working in this repo. Summarises the architecture, the
conventions, and where to find things. For user-facing docs see
[`README.md`](README.md) and [`docs/`](docs/README.md).

## What this is

`odr` (a.k.a. `odrcore`) is a **C++20 library that decodes documents and renders
them to HTML**. It reads many formats (ODF, OOXML, legacy MS, PDF, CSV, …) behind
one abstract document model and a generic HTML renderer. It is the backend for
OpenDocument.droid / .ios.

Build system: **CMake + Conan**. Language standard: **C++20** (`CMakeLists.txt`).

## Big picture: how a file becomes HTML

```
bytes ─▶ magic/open_strategy ─▶ DecodedFile ─▶ Document ─▶ ElementAdapter ─▶ html::translate ─▶ HtmlService
        (detect FileType +      (per engine)   (per       (tree of           (generic renderer,
         DecoderEngine)                          format)    elements)          walks public API)
```

1. **Detection** — `internal/magic.cpp` (+ `internal/libmagic`) sniffs the file;
   `internal/open_strategy.cpp` picks a `FileType` and a `DecoderEngine` and
   constructs the matching `abstract::DecodedFile`.
2. **Decode** — a document file yields an `abstract::Document` (the engine's
   subclass of `internal::Document`).
3. **Element tree** — a `Document` exposes a root `ElementIdentifier` plus an
   `abstract::ElementAdapter`. The public, value-semantics handles
   (`Element`, `Slide`, `Paragraph`, `Text`, `Frame`, …) in
   `src/odr/document_element.hpp` are thin wrappers that delegate to the adapter.
4. **Render** — `internal/html/` walks that public element API and writes HTML.
   Entry point: `odr::html::translate(...)` → `HtmlService` (paginated fragments;
   `bring_offline` materialises files).

### The element-adapter pattern (every document engine follows it)

Two pieces per engine:

- An **`ElementRegistry`**: a flat `std::vector<Element>` (id = index + 1) where
  each `Element` holds `parent`/`first_child`/`last_child`/`prev`/`next` ids and a
  `type`, plus side `unordered_map`s for per-type payloads (text strings, frame
  anchors, …). Builders are `create_element` / `create_*_element` /
  `append_child`. See `oldms/text/doc_element_registry.*` or
  `oldms/presentation/ppt_element_registry.*` for the minimal version.
- An **`ElementAdapter`**: one class that implements `abstract::ElementAdapter`
  (tree navigation by id) and, by multiple inheritance, the per-element-type
  adapters it supports (`SlideAdapter`, `ParagraphAdapter`, `TextAdapter`,
  `FrameAdapter`, …). The `*_adapter(id)` methods return `this` when the element
  is of that type, else `nullptr`. See `oldms/presentation/ppt_document.cpp` for a
  compact example.

`ElementType` is the shared enum in `src/odr/document_element.hpp` (`root`,
`slide`, `paragraph`, `text`, `line_break`, `frame`, `table*`, `sheet*`, …).

## Directory map

| Path | What |
|------|------|
| `src/odr/*.hpp` | **Public API**: `file.hpp`, `document.hpp`, `document_element.hpp`, `html.hpp`, `style.hpp`, `quantity.hpp` (`Measure`), `odr.hpp`. |
| `src/odr/internal/abstract/` | Core interfaces: `File`/`DecodedFile`, `Document` + `ElementAdapter` (and all per-element adapters), `Filesystem`, `Archive`, `HtmlService`. |
| `src/odr/internal/common/` | Reusable impls: `Path`/`AbsPath`, base `Document`, filesystem, `style`, table cursor/range, temp files. |
| `src/odr/internal/util/` | Helpers: `byte_stream_util` (POD reads), `string_util` (`split`, `u16string_to_string`), `stream_util`, `document_util`, `xml_util`. |
| `src/odr/internal/magic.*`, `open_strategy.*` | File-type detection and the open/dispatch logic. |
| `src/odr/internal/html/` | Generic HTML renderer (`document.cpp`, `document_element.cpp`, `document_style.cpp`). |
| `src/odr/internal/cfb/`, `zip/` | Container formats (Compound File Binary, ZIP). |
| `src/odr/internal/odf/` | OpenDocument (odt/ods/odp/odg). |
| `src/odr/internal/ooxml/` | OOXML (docx/pptx/xlsx); subdirs `text`/`presentation`/`spreadsheet`. |
| `src/odr/internal/oldms/` | **Legacy MS binary** (.doc/.ppt/.xls); subdirs `text`/`presentation`/`spreadsheet`. |
| `src/odr/internal/oldms_wvware/` | Alternative .doc decoder via wvWare. |
| `src/odr/internal/pdf/`, `pdf_poppler/` | PDF (own parser + poppler/pdf2htmlEX path). |
| `src/odr/internal/{csv,json,text,svm}/` | Smaller formats. |
| `cli/src/` | CLI tools: `translate`, `back_translate`, `meta`, `server`. |
| `test/src/` | GoogleTest suites; data in `test/data` (git submodules, see below). |
| `offline/documentation/MS-*/` | Vendored Microsoft spec text (PDF + extracted markdown), see [Specs](#specs). |
| `docs/design/README.md` | High-level design rationale. |

## Build & test

A configured build dir already exists (`cmake-build-debug`, also `…-release`,
`…-relwithdebinfo`). Typical loop:

```bash
# library
cmake --build cmake-build-debug --target odr
# tests (the ODR_TEST option is on in this build dir)
cmake --build cmake-build-debug --target odr_test
./cmake-build-debug/test/odr_test --gtest_filter='OldMs.*'
# CLI (renders a file to a directory of HTML)
cmake --build cmake-build-debug --target translate
```

Notable CMake options (`CMakeLists.txt`): `ODR_TEST`, `ODR_CLI`,
`ODR_WITH_PDF2HTMLEX`, `ODR_WITH_WVWARE`, `ODR_WITH_LIBMAGIC`, `ODR_CLANG_TIDY`.
A new `.cpp` must be added to the `ODR_SOURCE_FILES` list in `CMakeLists.txt`.

**Test data lives in git submodules** under `test/data/input/odr-public`,
`…/odr-private`, and `test/data/reference-output/*`.

## Conventions

- **Formatting**: clang-format, LLVM-based (`.clang-format`); run `scripts/format`
  (or rely on the git hook from `scripts/setup`). `clang-tidy` config in
  `.clang-tidy`; CI enforces both (`.github/workflows/format.yml`, `tidy.yml`).
- **Error handling — fail fast**: where the spec/format dictates what to expect,
  **throw** on unexpected input (`std::runtime_error`, or the typed exceptions in
  `src/odr/exceptions.hpp`) rather than silently degrading. Only **pass through**
  (return empty / skip) values that are genuinely *optional* or *not yet
  modelled*.
- **Public API**: value semantics; immutable handles; iterators only for
  immutable traversal (`docs/design/README.md`).
- **Byte parsing**: read POD structs via `util::byte_stream::read`; this assumes
  host byte order matches the file's (little-endian) — big-endian is a known
  not-yet-handled gap in the binary engines.
- Match the **surrounding file's** style, includes, and idioms; mirror a sibling
  engine when adding a format (the `oldms/text` `.doc` impl is the reference the
  `.ppt` impl was modelled on).
- **Comments — keep them minimal**: a function/struct doc comment is at most a
  couple of terse lines stating the key point (what it does, stream/ownership
  preconditions, the spec section it implements, e.g. `[MS-PPT] 2.3.2`). Don't
  restate the code or spell out every case; cite the spec instead of paraphrasing
  it. The detailed design rationale belongs in the per-module `AGENTS.md`, not in
  source comments.

## Adding / extending a document format

1. Detection: extend `magic`/`open_strategy` to map the bytes to a `FileType`
   (+ `DecoderEngine`) and construct your `DecodedFile`.
2. For documents: subclass `internal::Document`; in its constructor build an
   `ElementRegistry` and an `ElementAdapter` (see the pattern above).
3. Implement the per-element adapters you can populate; the **generic HTML
   renderer then works for free**.
4. Register the format's factory (e.g. `oldms_file.cpp::document()` switches on
   `file_type()`), add sources to `CMakeLists.txt`, and add a GoogleTest.

## Legacy Microsoft binary formats (`oldms`)

Container handling (CFB) already exists; each format is a small module under
`oldms/` mirroring `oldms/text` (`.doc`). Spec references in
`src/odr/internal/oldms/README.md`.

- **`.doc`** (`oldms/text`): working, visible-text extraction.
- **`.ppt`** (`oldms/presentation`): implemented — slides resolved via the
  persist directory (the only spec-defined read path), each slide's text boxes
  modelled as positioned `frame`s. **Read its docs before touching it**:
  [`oldms/presentation/AGENTS.md`](src/odr/internal/oldms/presentation/AGENTS.md)
  — what's implemented and **why** (persist-directory resolution, no scan
  fallback, sequential `ChildCursor` reading without `tellg`, fail-fast error
  handling, the two-text-locations finding, endianness), the open work (frame
  refinements, smaller shortcomings), and the verified `[MS-PPT]`/`[MS-ODRAW]`
  drawing-tree map.
- **`.xls`** (`oldms/spreadsheet`): working, visible cell-text extraction
  (BIFF8). See [`oldms/spreadsheet/AGENTS.md`](src/odr/internal/oldms/spreadsheet/AGENTS.md).

## Specs

Vendored Microsoft Open Specifications live under
`offline/documentation/<NAME>/<NAME>-<date>/`, both as `original.pdf` and an
extracted `docling-from-docx.md` (grep-friendly). Available: **MS-PPT**,
**MS-ODRAW** (Office Art / Escher drawing records), **MS-DOC**, **MS-XLS**,
**MS-CFB** (container), **MS-OFFCRYPTO** (encryption). Cite section numbers from
these when implementing binary parsing.
