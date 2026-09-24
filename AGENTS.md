# AGENTS.md

Orientation for agents: architecture, directory map, build loop, conventions,
releasing. User-facing docs are in [`README.md`](README.md). A module with its
own `AGENTS.md` carries the rules for that module. Read it before you touch the
module.

## What this is

`odr` (also `odrcore`) is a C++23 library. It decodes documents and renders
them to HTML. Every format sits behind one abstract document model and one
generic HTML renderer. Build: CMake plus Conan.

## How a file becomes HTML

```
bytes ─▶ magic / open_strategy ─▶ DecodedFile ─▶ Document ─▶ ElementAdapter ─▶ html::translate ─▶ HtmlService
```

1. Detect. `internal/magic.cpp` sniffs the head of the file.
   `internal/open_strategy.cpp` picks a `FileType` and a `DecoderEngine` and
   builds the `abstract::DecodedFile`. Only bytes can claim a file. A name adds
   a candidate that the bytes allow. That is the only way in for a format with
   no signature (markdown).
2. Decode. A document file yields an `abstract::Document`.
3. Element tree. A `Document` exposes a root `ElementIdentifier` and an
   `abstract::ElementAdapter`. The public handles in `src/odr/document_element.hpp`
   (`Element`, `Paragraph`, `Text`, `Frame`, ...) delegate to the adapter.
4. Render. `internal/html/` walks the public element API and writes HTML.
   Entry: `odr::html::translate(...)` returns an `HtmlService`.

### The element-adapter pattern

The machinery is shared. Do not write it again. It lives in
`internal/common/element_registry.hpp` and `internal/common/element_adapter.hpp`.
A compact example is `rtf/rtf_element_registry.*` plus `rtf/rtf_document.cpp`.

- `internal::ElementRegistry<Element, Id>` is the flat store. The id is the
  index plus one. The engine derives `struct RegistryElement final :
  ElementNode<Id>` and adds a field only if it has one. Per-type payloads are
  `SideTable<T>` (hashed) or `SortedSideTable<T, Id>` (binary search, for
  payloads written in id order). Both check their bounds, so an accessor is
  `return m_texts.at(id);` and nothing else.
- `ElementIdentifier` is opaque and 64 bits wide. Registry engines use index
  plus one. `csv` packs a kind, a row and a column into it. An engine that wants
  a narrower id passes it as the registry's `Id` (`odf::StoredId`) and widens
  at the boundary.
- `internal::RegistryElementAdapter<Registry, Adapters...>` implements the tree
  navigation. Its base `internal::ElementAdapter<Adapters...>` answers every
  `*_adapter(id)` hook from the adapters in the pack. An engine lists the
  adapters and writes no hook. `element_is_unique` and
  `element_is_self_locatable` default to true, `element_is_editable` to false.
  Override only where that is wrong. An engine with no registry (`csv`) derives
  from `internal::ElementAdapter` and writes its own navigation.

Two model rules that every engine follows:

- Every drawing is a `frame`. `Frame::shape_type` is `none` for a plain box,
  else `rect`, `ellipse`, `line` or `custom` with its own `Frame::path`. Only
  odf sets a shape. The other engines funnel every shape into a plain frame.
- A manual page break is `ParagraphStyle::break_before` / `break_after` for
  odf and ooxml, and `ElementType::page_break` for rtf and `.doc`. The renderer
  splits the page box on both. Automatic breaks that a producer recorded
  (`text:soft-page-break`) are not parsed.

## Directory map

| Path | What |
|------|------|
| `src/odr/*.hpp` | Public API: `file`, `document`, `document_element`, `html`, `style`, `quantity`, `odr`. |
| `src/odr/internal/abstract/` | Core interfaces: `File`, `DecodedFile`, `Document`, `ElementAdapter`, `Filesystem`, `Archive`, `HtmlService`. |
| `src/odr/internal/common/` | Shared implementations: `Path`, base `Document`, `ElementRegistry`, `ElementAdapter`, filesystem, style, table cursor, `TextCursor`, `SheetDependencies`, temp files. |
| `src/odr/internal/util/` | Helpers: byte stream, string, stream, document, number. |
| `src/odr/internal/magic.*`, `open_strategy.*` | Detection and open dispatch. |
| `src/odr/internal/file_type_table.*` | The one table per `FileType`: extensions, MIME types, category, document type, capabilities. Every lookup in `odr.hpp` forwards into it. Extend the table, not the lookups. |
| `src/odr/internal/formula/` | Spreadsheet formulas. One AST, parsed from OpenFormula and OOXML, shared by odf and ooxml. |
| `src/odr/internal/html/` | The generic HTML renderer. |
| `src/odr/internal/html/frontend/` | The stylesheets and scripts the renderer embeds. `cmake/frontend_assets.cmake` embeds them. `frontend.cpp` decides which view writes which. |
| `src/odr/internal/cfb/`, `zip/` | Containers. |
| `src/odr/internal/odf/` | OpenDocument. See [`odf/AGENTS.md`](src/odr/internal/odf/AGENTS.md). |
| `src/odr/internal/ooxml/` | Office Open XML. See [`ooxml/AGENTS.md`](src/odr/internal/ooxml/AGENTS.md) and the per-format docs. |
| `src/odr/internal/oldms/` | Legacy Microsoft binary (`.doc`, `.ppt`, `.xls`). Visible text only. See [`oldms/AGENTS.md`](src/odr/internal/oldms/AGENTS.md) and the per-format docs. |
| `src/odr/internal/iwork/` | Apple iWork (`.pages`, `.key`, `.numbers`). No spec; fixtures are the citation. See [`iwork/AGENTS.md`](src/odr/internal/iwork/AGENTS.md) and [`iwork/PLAN.md`](src/odr/internal/iwork/PLAN.md). |
| `src/odr/internal/pdf/` | PDF, own parser. See [`pdf/AGENTS.md`](src/odr/internal/pdf/AGENTS.md). |
| `src/odr/internal/png/` | PNG encoder for `pdf` image extraction and `svm` bitmaps. |
| `src/odr/internal/rtf/` | RTF, read as a text document. See [`rtf/AGENTS.md`](src/odr/internal/rtf/AGENTS.md) and [`rtf/PLAN.md`](src/odr/internal/rtf/PLAN.md). |
| `src/odr/internal/markdown/` | Markdown (md4c), read as a text document. See [`markdown/AGENTS.md`](src/odr/internal/markdown/AGENTS.md) and [`markdown/PLAN.md`](src/odr/internal/markdown/PLAN.md). |
| `src/odr/internal/xml/` | The pugixml parse, the escaping every xml writer shares (`xml_util`), and the source view. See [`xml/AGENTS.md`](src/odr/internal/xml/AGENTS.md). |
| `src/odr/internal/svg/` | SVG. See [`svg/AGENTS.md`](src/odr/internal/svg/AGENTS.md). |
| `src/odr/internal/svm/` | StarView metafile, translated to svg. See [`svm/AGENTS.md`](src/odr/internal/svm/AGENTS.md). |
| `src/odr/internal/{csv,json,text}/` | Smaller formats. See [`csv/AGENTS.md`](src/odr/internal/csv/AGENTS.md). |
| `cli/src/` | CLI tools: `translate`, `back_translate`, `meta`, `server`. |
| `python/`, `jni/`, `android/`, `apple/`, `wasm/` | Bindings. Each has an `AGENTS.md`. |
| `tools/pdf/` | Generators for the committed PDF encoding data. Not built. See [`tools/pdf/README.md`](tools/pdf/README.md). |
| `test/src/` | GoogleTest suites. |
| `test/browser/` | Checks for the embedded scripts, run in a browser, not by `odr_test`. Each directory has a `README.md`. |
| `offline/documentation/` | Vendored specs. See [Specs](#specs). |
| `docs/design/` | Design decisions. |

## Build and test

Configured build dirs exist. The loop:

```bash
cmake --build cmake-build-relwithdebinfo --target odr        # library
cmake --build cmake-build-relwithdebinfo --target odr_test   # tests (ODR_TEST=ON)
(cd cmake-build-relwithdebinfo && ./test/odr_test --gtest_filter='OldMs.*')
cmake --build cmake-build-relwithdebinfo --target translate  # CLI: file to HTML dir
```

- Use `cmake-build-relwithdebinfo`, not `cmake-build-debug`.
- Run a targeted `--gtest_filter`. The full suite is slow.
- Run the test binary from the build dir, so output stays out of the repo.
- To debug one file, use the `translate` CLI.
- CMake options: `ODR_TEST`, `ODR_TEST_FETCH_DATA`, `ODR_CLI`,
  `ODR_WITH_HTTP_SERVER`, `ODR_CLANG_TIDY`, `ODR_PYTHON`, `ODR_JNI`,
  `ODR_APPLE`, `ODR_WASM`. Add a new `.cpp` to `ODR_SOURCE_FILES`.
- Test data is fetched, not vendored. `-DODR_TEST_FETCH_DATA=ON` makes
  `cmake/test_data.cmake` clone the repositories pinned in `test/data.cmake`
  into `test/data/`. It is off by default because the data is several
  gigabytes and `odr_test` builds without it. The `update_test_data` target
  moves existing checkouts onto the pins. Two repositories are private.
- pugixml is built with `PUGIXML_COMPACT`. The odf, ooxml, svg and xml engines
  keep the parsed DOM as their backing store, so the node size is the document
  size. The define changes the ABI, and a mismatch between translation units
  corrupts silently. So `odr` carries it as an INTERFACE define, `conanfile.py`
  declares it in `package_info`, and a new target that includes `pugixml.hpp`
  must get it too. `xml/xml_util.cpp` asserts the layout.

## Conventions

- Formatting: clang-format per `.clang-format`, clang-tidy per `.clang-tidy`.
  Run `scripts/format`, or install the hook with `scripts/setup`. CI enforces
  both.
- C++23 with three ceilings. Check a new facility against all three by building
  an object file, because `-fsyntax-only` misses codegen bugs.
  - The standard library is capped by emsdk 3.1.73 (libc++ 18.1). Available:
    `std::ranges::to`, `std::expected`, `std::string::resize_and_overwrite`,
    `views::zip`, `ranges::fold_left`, monadic `std::optional`. Not available:
    `views::enumerate`, `std::generator`, `std::move_only_function`,
    `std::flat_map`. `std::mdspan` is missing in libstdc++ (the gcc-14 job).
  - `std::format` is unusable. It needs a floating-point `std::to_chars` that
    macOS 13.3 and iOS 16.3 introduced, and the apple slices deploy to macOS 12
    and iOS 15. Use `fmt`.
  - Deducing `this` works on an accessor, but not on a recursive capturing
    lambda. NDK 28.1's clang 19 crashes on it. Pass the lambda to itself
    instead, as `ooxml_text_list.cpp` does.
- The public headers stay C++20. Nothing propagates the standard to a consumer:
  no `target_compile_features(odr PUBLIC ...)`, no `cppstd` in `package_info`.
  C++23 is for `internal/`, `cli/` and the bindings.
- Fail fast. Where the spec says what to expect, throw on unexpected input
  (`std::runtime_error` or the typed exceptions in `src/odr/exceptions.hpp`).
  Pass through only values that are optional or not yet modelled.
- Format numbers with `fmt`, never a stream. A stream carries the host locale,
  and a German one writes `1,5` into a css length.
  `util::number::to_string_significant` is the css and svg spelling.
- Use fixed-width integer types from `<cstdint>`. Use `int` and friends only
  for index-like values (`std::size_t`) or where an API forces them.
- Use `std::array`, never a C array.
- Mark locals, parameters and members `const` where they do not mutate.
- Prefer `std::ranges` algorithms and range-based loops over iterator pairs.
- Define a header-declared free function with its qualified name inside the
  reopened namespace, never as a bare redeclaration. A signature that drifts
  from the header then fails at compile time. Keep file-local helpers in an
  anonymous namespace.
- A class declared in a `.cpp` defines its members inline. A class with a
  header keeps the usual split.
- The input file never authors the output markup. Text goes through
  `escape_text`, images become an `<img>` we construct, and an svg goes out as
  a data url. Every `href` goes through `html::uri_kind` in `html/common.cpp`.
  A refused target loses its `href`, an external one gets `target="_blank"`, a
  relative one no target. No view declares a document-wide `<base target>`.
- Public API: value semantics, immutable handles, iterators only for immutable
  traversal.
- Byte parsing reads POD structs with `util::byte_stream::read`. It assumes a
  little-endian host. Big-endian is a known gap.
- Match the surrounding file. Mirror a sibling engine when you add a format.
- Comments are minimal. A doc comment states the key point in one or two lines:
  what it does, preconditions, the spec section (`[MS-PPT] 2.3.2`). Do not
  restate the code. Cite the spec, do not paraphrase it. Rationale goes in the
  module's `AGENTS.md`.
- Doc-comment markers: `///` for functions, classes, structs and enums.
  Trailing `///<` for a member or enumerator. `@brief` only where a detail
  paragraph follows it.
- Pull requests: put the `🤖 Generated with [Claude Code](https://claude.com/claude-code)`
  line at the top of the body. A change a consumer notices gets a
  `CHANGELOG.md` entry in the same PR.

## Adding a document format

1. Detection: extend `magic` and `open_strategy`, and add a row to
   `internal/file_type_table.cpp`. `odr_test` fails if a `FileType` has no
   row, if an alias is claimed twice, or if the declared capabilities exceed
   what the engine does.
2. Subclass `internal::Document`. In the constructor build an `ElementRegistry`
   and an `ElementAdapter` as described above. The document is read-only by
   default. Override `is_editable`, `is_savable` and `save` only for an engine
   that writes.
3. Implement the per-element adapters you can populate. The HTML renderer then
   works without changes.
4. Register the factory (for example `oldms_file.cpp::document()`), add the
   sources to `CMakeLists.txt`, add a GoogleTest.

## Releasing

Dispatch `release.yml` against main, then publish the draft release that
appears. The scripts are `scripts/release.py` and `scripts/release_status.py`.
Both run by hand, and `--dry-run` mutates nothing.

- Dispatch is the only trigger. The first job refuses a ref that is not main
  or `release/**`. `dry_run` runs everything without a push.
- `main` carries no version. `git cliff --bumped-version` derives it from the
  commit subjects, so write them properly. `release.py version` answers the
  same question locally.
- `CHANGELOG.md` is written as the changes land, under `## Unreleased`, in the
  pull request. The file header says what earns an entry. An empty
  `## Unreleased` fails the first job. Write "no consumer-visible changes"
  rather than nothing.
- To patch an older line, branch off the tag (`git branch release/v6.1.X
  v6.1.0`) and dispatch against that. A `feat:` there bumps the minor to a
  number main may have shipped, so `release.py version` refuses a version that
  is already tagged. Pass `--version` when you mean it. That changelog is not
  merged back.
- A human publishes the draft. GitHub creates the tag at publish. A release
  created by `GITHUB_TOKEN` raises no `release: published` event, and that
  event starts the conan, maven and android workflows.
- `release.py stamp` commits the version to main as `chore(release): vX.Y.Z
  [skip ci]`. Today that is `Package.swift` and the changelog heading. The push
  needs a token from a GitHub App on the bypass list (`RELEASE_APP_CLIENT_ID`,
  `RELEASE_APP_PRIVATE_KEY`), because `GITHUB_TOKEN` cannot bypass the main
  ruleset.
- A job attaches something to the release by naming its artifact
  `release-asset-*`.
- `release_status.yml` waits for the publish workflows and fails if one failed
  or never started. `EXPECTED` in `scripts/release_status.py` lists the
  destinations.

## Specs

Vendored specifications live under `offline/documentation/<NAME>/<NAME>-<date>/`
as `original.pdf` plus a markdown conversion (`docling-from-docx.md` where it
exists). See [`offline/documentation/README.md`](offline/documentation/README.md)
for the list: MS-CFB, MS-DOC, MS-DOCX, MS-ODRAW, MS-OFFCRYPTO, MS-PPT, MS-PPTX,
MS-XLS, MS-XLSX, MSFT-RTF and PDF. Cite section numbers when you implement
binary parsing.
