# AGENTS.md — OpenDocument.core

Orientation for AI agents. Architecture, conventions, and where things live. For
user-facing docs see [`README.md`](README.md) and [`docs/`](docs/README.md).

## What this is

`odr` (a.k.a. `odrcore`) is a **C++20 library that decodes documents and renders
them to HTML**. It reads many formats (ODF, OOXML, legacy MS binary, PDF, CSV, …)
behind one abstract document model and a generic HTML renderer. It is the backend
for OpenDocument.droid / .ios. Build: **CMake + Conan**; standard: **C++20**.

## Big picture: how a file becomes HTML

```
bytes ─▶ magic/open_strategy ─▶ DecodedFile ─▶ Document ─▶ ElementAdapter ─▶ html::translate ─▶ HtmlService
        (detect FileType +      (per engine)   (per       (tree of           (generic renderer,
         DecoderEngine)                          format)    elements)          walks public API)
```

1. **Detect** — `internal/magic.cpp` sniffs the head of the file;
   `internal/open_strategy.cpp` picks a `FileType` + `DecoderEngine` and builds
   the matching `abstract::DecodedFile`. `odr::mimetype` composes the two, so a
   zip is named by what is inside it.
2. **Decode** — a document file yields an `abstract::Document`.
3. **Element tree** — a `Document` exposes a root `ElementIdentifier` plus an
   `abstract::ElementAdapter`. Public value-semantics handles (`Element`, `Slide`,
   `Paragraph`, `Text`, `Frame`, …) in `src/odr/document_element.hpp` are thin
   wrappers that delegate to the adapter.
4. **Render** — `internal/html/` walks the public element API and writes HTML.
   Entry: `odr::html::translate(...)` → `HtmlService` (paginated fragments;
   `bring_offline` materialises files).

### The element-adapter pattern (every engine follows it)

- An **`ElementRegistry`**: a flat `std::vector<Element>` (id = index + 1); each
  `Element` holds `parent`/`first_child`/`last_child`/`prev`/`next` ids and a
  `type`, plus side maps for per-type payloads. Builders: `create_element` /
  `create_*_element` / `append_child`. Minimal example:
  `oldms/presentation/ppt_element_registry.*`.
- An **`ElementIdentifier` is opaque, and 64 bits wide**: registry engines use it
  as index + 1, `csv` packs a kind, a row and a column into its bits. An engine
  wanting a narrower id narrows it inside its own store (`odf`'s
  `ElementRegistry::StoredId`) and widens at the boundary.
- An **`ElementAdapter`**: one class implementing `abstract::ElementAdapter` (tree
  navigation by id) and, via multiple inheritance, the per-type adapters it
  supports (`SlideAdapter`, `ParagraphAdapter`, …). Each `*_adapter(id)` returns
  `this` when the element is that type, else `nullptr`. Compact example:
  `oldms/presentation/ppt_document.cpp`.

`ElementType` is the shared enum in `src/odr/document_element.hpp`.

## Directory map

| Path | What |
|------|------|
| `src/odr/*.hpp` | **Public API**: `file`, `document`, `document_element`, `html`, `style`, `quantity` (`Measure`), `odr`. |
| `src/odr/internal/abstract/` | Core interfaces: `File`/`DecodedFile`, `Document` + `ElementAdapter`, `Filesystem`, `Archive`, `HtmlService`. |
| `src/odr/internal/common/` | Reusable impls: `Path`/`AbsPath`, base `Document`, filesystem, `style`, table cursor/range, temp files. |
| `src/odr/internal/util/` | Helpers: `byte_stream_util`, `string_util`, `stream_util`, `document_util`. |
| `src/odr/internal/magic.*`, `open_strategy.*` | File-type detection + open/dispatch. |
| `src/odr/internal/file_type_table.*` | **The** per-`FileType` table: extensions, MIME types, category, document type, `FileTypeCapabilities`. Every public lookup in `odr.hpp` is a thin forward into it — extend the table, not the lookups. |
| `src/odr/internal/html/` | Generic HTML renderer. |
| `src/odr/internal/cfb/`, `zip/` | Container formats (CFB, ZIP). |
| `src/odr/internal/odf/` | OpenDocument (odt/ods/odp/odg); see [`odf/AGENTS.md`](src/odr/internal/odf/AGENTS.md). |
| `src/odr/internal/ooxml/` | OOXML (docx/pptx/xlsx); see [`ooxml/AGENTS.md`](src/odr/internal/ooxml/AGENTS.md) + per-format docs. |
| `src/odr/internal/oldms/` | **Legacy MS binary** (.doc/.ppt/.xls). |
| `src/odr/internal/iwork/` | Apple iWork (`.pages`, `.key`, `.numbers`); see [`iwork/AGENTS.md`](src/odr/internal/iwork/AGENTS.md) + [`iwork/PLAN.md`](src/odr/internal/iwork/PLAN.md). |
| `src/odr/internal/pdf/` | PDF (own parser). |
| `src/odr/internal/png/` | PNG encoder: the writer `pdf` image extraction and `svm` bitmaps hand their pixels to. |
| `src/odr/internal/rtf/` | RTF, read as a text document; see [`rtf/AGENTS.md`](src/odr/internal/rtf/AGENTS.md) + [`rtf/PLAN.md`](src/odr/internal/rtf/PLAN.md). |
| `src/odr/internal/markdown/` | Markdown (CommonMark + GFM via md4c), decoded to a text document; see [`markdown/AGENTS.md`](src/odr/internal/markdown/AGENTS.md) + [`markdown/PLAN.md`](src/odr/internal/markdown/PLAN.md). |
| `src/odr/internal/xml/` | XML: the pugixml parse and the escaping every xml-writing engine shares (`xml_util`), plus the source view (`xml_file`); see [`xml/AGENTS.md`](src/odr/internal/xml/AGENTS.md). |
| `src/odr/internal/svg/` | SVG, detected by reading it as xml; see [`svg/AGENTS.md`](src/odr/internal/svg/AGENTS.md). |
| `src/odr/internal/svm/` | StarView metafile, the vector image odf/ooxml packages carry for charts and OLE objects; translated to svg. See [`svm/AGENTS.md`](src/odr/internal/svm/AGENTS.md) + [`svm/PLAN.md`](src/odr/internal/svm/PLAN.md). |
| `src/odr/internal/{csv,json,text}/` | Smaller formats. |
| `cli/src/` | CLI tools: `translate`, `back_translate`, `meta`, `server`. |
| `python/` | Python bindings (`pyodr`, pybind11); see [`python/AGENTS.md`](python/AGENTS.md). |
| `jni/` | JNI bindings (Java package `app.opendocument.core`); see [`jni/AGENTS.md`](jni/AGENTS.md). |
| `android/` | The bindings packaged as an AAR (`odr-core-android`) + the instrumented tests; see [`android/AGENTS.md`](android/AGENTS.md). |
| `apple/` | Objective-C bindings + the Swift package, shipped as `OdrCoreObjC.xcframework`; see [`apple/AGENTS.md`](apple/AGENTS.md). |
| `wasm/` | WebAssembly bindings (embind), packaged as the npm package `@opendocument/odr-core`; see [`wasm/AGENTS.md`](wasm/AGENTS.md). |
| `tools/pdf/` | Dev tooling (not built): PDF encoding-data generators, see `tools/pdf/README.md`. |
| `test/src/` | GoogleTest suites; data fetched into `test/data` (see `cmake/test_data.cmake`). |
| `test/browser/` | Checks for the emitted scripts, run by hand in a browser — what they do is not visible to `odr_test`; see [`viewport/README.md`](test/browser/viewport/README.md). |
| `offline/documentation/MS-*/` | Vendored Microsoft spec text (see [Specs](#specs)). |
| `docs/design/README.md` | High-level design rationale. |

## Build & test

Configured build dirs already exist. Typical loop:

```bash
cmake --build cmake-build-relwithdebinfo --target odr        # library
cmake --build cmake-build-relwithdebinfo --target odr_test   # tests (ODR_TEST on)
(cd cmake-build-relwithdebinfo && ./test/odr_test --gtest_filter='OldMs.*')
cmake --build cmake-build-relwithdebinfo --target translate  # CLI: file → HTML dir
```

- **Default to `cmake-build-relwithdebinfo`** (not `…-debug`).
- **Run only a targeted `--gtest_filter`**; the full suite is slow — run it only
  when really necessary.
- **Run the test binary from the build dir** so output stays out of the repo tree.
- **For debugging, prefer the `translate` CLI** on a single file over the suite.
- CMake options (`CMakeLists.txt`): `ODR_TEST`, `ODR_CLI`, `ODR_PYTHON`,
  `ODR_JNI`, `ODR_APPLE`, `ODR_WASM`, `ODR_CLANG_TIDY`. A new `.cpp` must be added to
  `ODR_SOURCE_FILES`.
- **Test data is fetched, not vendored**, and opt in: `-DODR_TEST_FETCH_DATA=ON`
  makes `cmake/test_data.cmake` clone the repositories pinned in
  `test/data.cmake` into `test/data/` (they were submodules until then; read
  that file's header for why). Off by default — building `odr_test` does not
  need the data, and several gigabytes should not arrive because someone turned
  tests on. The `update_test_data` target moves existing checkouts onto the
  pins. The two private repositories need credentials.
- **pugixml is built in compact mode** (`PUGIXML_COMPACT`, set on the imported
  target in `CMakeLists.txt`, paired with `pugixml/*:header_only` in
  `conanfile.py`). The odf/ooxml/svg/xml engines keep the parsed DOM resident as
  their backing store, so its size *is* the document's: a 12-byte node instead
  of 64 took a 297 MB `content.xml` from 594 MB to 116 MB. The define is ABI
  affecting and mixing it across translation units is silent corruption, not a
  link error — hence the imported target, and no prebuilt library to mismatch
  against. A new target that includes `pugixml.hpp` has to get it too, and the
  installed internal headers expose pugixml types, so `odr` carries the define
  INTERFACE and `package_info` declares it. `xml/xml_util.cpp` asserts the
  layout it compiled against.

## Releasing

Dispatch `release.yml` against main, publish the draft that appears —
`.github/workflows/release.yml` and `scripts/release.py`.

- **Dispatch is the only trigger**, and the first job refuses a ref that is
  neither main nor `release/**`. `dry_run` goes through the motions without
  pushing — the rehearsal for the release body.
- **`main` carries no version in anything the build reads.** No file is bumped
  by hand; a build records `GIT_HEAD_SHA1` and a dirty flag and nothing else.
  The version is derived from the commit subjects (`git cliff
  --bumped-version`), so writing them properly is load-bearing.
  `scripts/release.py version` answers the same question locally.
- **`CHANGELOG.md` is written as the changes land**, under `## Unreleased`, in
  the pull request that makes them; the file's header says what earns an entry.
  The run heads them with the version and puts them above the generated commit
  list. An empty `## Unreleased` fails the first job — say "no consumer-visible
  changes" rather than nothing.
- **To patch an older line, branch off the *tag*** (`git branch release/v6.1.X
  v6.1.0`) and dispatch against that — the version is derived against the
  nearest *reachable* tag. That is also the trap: a `feat:` there bumps the
  minor to a number the mainline may already have shipped, so
  `release.py version` refuses a version that is already tagged. Pass
  `--version` when you mean it. Its changelog covers what that branch contains
  and is not merged back; a cherry-pick brings the entry with it.
- **The release is drafted, and a human publishes it.** GitHub creates the tag
  only then, which is what lets it point at a commit made during the run. It
  also has to be a human: a release created by `GITHUB_TOKEN` raises no
  `release: published`, and that event starts conan, maven and android.
- **`release.yml` is the only place that writes a version into the build**, and
  `release.py stamp` commits it to main as `chore(release): vX.Y.Z`. Today that
  is `Package.swift` and the changelog heading: SwiftPM resolves the manifest at
  the tag, and its binary target names the sha256 of an archive that does not
  exist until the release builds it. That push cannot use `GITHUB_TOKEN` —
  GitHub Actions cannot be a bypass actor on the main ruleset — so the job mints
  one from a GitHub App that is on the bypass list (`RELEASE_APP_CLIENT_ID`,
  `RELEASE_APP_PRIVATE_KEY`). An app token does raise events, hence `[skip ci]`
  in the stamp subject.
- **A job that wants something attached to the release** names its artifact
  `release-asset-*`; `release.yml` uploads those and nothing else.
- **`release-status.yml` makes a partial release loud** — it waits for the
  publish workflows and fails if one failed or never started. `EXPECTED` in
  `scripts/release_status.py` is the list of destinations.
- Both scripts are runnable by hand; `--dry-run` mutates nothing.

## Conventions

- **Formatting**: clang-format (LLVM-based, `.clang-format`); run `scripts/format`
  or use the `scripts/setup` git hook. `clang-tidy` per `.clang-tidy`. CI enforces
  both.
- **Fail fast**: where the spec dictates what to expect, **throw** on unexpected
  input (`std::runtime_error` or the typed exceptions in `src/odr/exceptions.hpp`)
  rather than silently degrading. Only pass through (return empty / skip) values
  that are genuinely *optional* or *not yet modelled*.
- **Fixed-width integer types — always**: prefer `<cstdint>` types (`std::int32_t`,
  `std::uint8_t`, …) over `int` / `unsigned` / `long` / `unsigned char`. Reserve
  the built-in types for genuinely index/size-like values (`std::size_t`) or where
  an API forces them.
- **`std::array` over C arrays — always**: never declare a C-style `T[N]`; use
  `std::array<T, N>`.
- **`const` where possible**: mark locals, parameters, and members `const` whenever
  they don't need to mutate.
- **Prefer ranges**: use `std::ranges` algorithms and range-based overloads over
  iterator pairs (`std::ranges::find_if(v, …)` not `std::find_if(v.begin(), …)`),
  and prefer range views/`for (auto &x : range)` over manual iterator loops. Fall
  back to iterator pairs only where a range overload genuinely doesn't fit
  (e.g. reverse-iterator `.base()` tricks).
- **Bind free-function definitions to a namespace**: in a source file, define a
  header-declared free function (or util struct's static method) with its
  **qualified** name — the `Ret ns::fn(...)` / `Ret Struct::fn(...)` form, inside
  the reopened `namespace` — never as a bare unqualified redeclaration. A qualified
  out-of-line definition must match an existing declaration, so a signature that
  drifts from the header fails at **compile** time instead of becoming an obscure
  linker error. (The `util` helpers use the `struct string { static … }` idiom for
  exactly this.) Keep translation-unit-local helpers in an **anonymous namespace**.
- **A source-only class defines its members inline**: a class declared in a
  `.cpp` — an anonymous-namespace helper, an `ElementAdapter` — has no header to
  keep honest, so put every body in the class body. The declare-then-define
  split buys nothing there and doubles what a reader has to line up. A class
  with a header keeps the usual split.
- **The input file never authors the output markup**: we interpret a file and
  emit our own html — text through `escape_text`, images as an `<img>` we
  construct. Nothing is passed through as live markup, which is why an svg goes
  out as a data url rather than inlined ([`svg/AGENTS.md`](src/odr/internal/svg/AGENTS.md))
  and why the rendered page needs no sanitiser. A link target goes through one
  classifier, `html::uri_kind` in `html/common.cpp`, called by both a PDF
  `/URI` action and `html/document_element.cpp::translate_link`: a refused
  target loses its `href` and keeps its text, an external one gets
  `target="_blank"`, a relative one no target. No view declares a document-wide
  `<base target>`. A third writer of an `href` calls it too.
- **Public API**: value semantics; immutable handles; iterators only for immutable
  traversal (`docs/design/README.md`).
- **Byte parsing**: read POD structs via `util::byte_stream::read`; assumes host
  byte order matches the file's (little-endian) — big-endian is a known gap.
- **Match the surrounding file** and mirror a sibling engine when adding a format
  (the `oldms/text` `.doc` impl is the reference the `.ppt` impl was modelled on).
- **Comments — minimal**: a doc comment is at most a couple of terse lines stating
  the key point (what it does, stream/ownership preconditions, spec section, e.g.
  `[MS-PPT] 2.3.2`). Don't restate the code; cite the spec, don't paraphrase it.
  Detailed rationale belongs in the per-module `AGENTS.md`.
- **Doc-comment markers**: `///` for functions/classes/structs/enums; trailing
  `///<` for the short note on the same line (enumerator/member). Keep terse.
- **Pull requests**: put the `🤖 Generated with [Claude Code](https://claude.com/claude-code)`
  line **at the top** of the PR body. If the change is one a consumer would
  notice, add its `CHANGELOG.md` entry in the same PR — see *Releasing*.

## Adding / extending a document format

1. Detection: extend `magic`/`open_strategy` to map bytes → `FileType` and
   construct your `DecodedFile`, and add a row to
   `internal/file_type_table.cpp` (extensions, MIME types, category, document
   type, capabilities) — `odr_test` fails if a `FileType` has no row, if an
   alias is claimed twice, or if the declared capabilities exceed what the
   engines actually do.
2. For documents: subclass `internal::Document`; in its constructor build an
   `ElementRegistry` and an `ElementAdapter` (pattern above).
3. Implement the per-element adapters you can populate; the **generic HTML
   renderer then works for free**.
4. Register the factory (e.g. `oldms_file.cpp::document()` switches on
   `file_type()`), add sources to `CMakeLists.txt`, add a GoogleTest.

## Apple iWork (`iwork`)

`.pages` opens as a text document, `.key` as a presentation and `.numbers` as a
spreadsheet. There is no spec — the module cites fixtures instead, keeps its own
Snappy and protobuf readers, and fails soft on archive types it has not mapped.
Archive type ids are namespaced per app, so a `.key` and a `.numbers` share them
and the component list is what tells the two apart. Tables everywhere go
through one tile reader, and a cell value stays the decimal the file stores. Read [`iwork/AGENTS.md`](src/odr/internal/iwork/AGENTS.md)
before touching it, and [`iwork/PLAN.md`](src/odr/internal/iwork/PLAN.md) for
what comes next.

## Legacy Microsoft binary formats (`oldms`)

CFB container handling exists; each format is a small module under `oldms/`
mirroring `oldms/text` (`.doc`), all doing visible-text extraction only. Shared
conventions + the endianness analysis are in
[`oldms/AGENTS.md`](src/odr/internal/oldms/AGENTS.md); spec refs in
`src/odr/internal/oldms/README.md`. **Read the module's own `AGENTS.md` before
touching it** — each carries the design rationale, spec-record maps, and open work:
`text/` (`.doc`), `presentation/` (`.ppt`, BIFF-style drawing tree),
`spreadsheet/` (`.xls`, BIFF8).

## Specs

Vendored Microsoft Open Specifications under
`offline/documentation/<NAME>/<NAME>-<date>/` as `original.pdf` + an extracted
`docling-from-docx.md` (grep-friendly). Available: **MS-PPT**, **MS-ODRAW**,
**MS-DOC**, **MS-XLS**, **MS-CFB**, **MS-OFFCRYPTO**. Cite section numbers when
implementing binary parsing.
