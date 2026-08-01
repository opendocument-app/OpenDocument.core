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

1. **Detect** — `internal/magic.cpp` (+ `internal/libmagic`) sniffs the file;
   `internal/open_strategy.cpp` picks a `FileType` + `DecoderEngine` and builds
   the matching `abstract::DecodedFile`.
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
| `src/odr/internal/util/` | Helpers: `byte_stream_util`, `string_util`, `stream_util`, `document_util`, `xml_util`. |
| `src/odr/internal/magic.*`, `open_strategy.*` | File-type detection + open/dispatch. |
| `src/odr/internal/file_type_table.*` | **The** per-`FileType` table: extensions, MIME types, category, document type, `FileTypeCapabilities`. Every public lookup in `odr.hpp` is a thin forward into it — extend the table, not the lookups. |
| `src/odr/internal/html/` | Generic HTML renderer. |
| `src/odr/internal/cfb/`, `zip/` | Container formats (CFB, ZIP). |
| `src/odr/internal/odf/` | OpenDocument (odt/ods/odp/odg); see [`odf/AGENTS.md`](src/odr/internal/odf/AGENTS.md). |
| `src/odr/internal/ooxml/` | OOXML (docx/pptx/xlsx); see [`ooxml/AGENTS.md`](src/odr/internal/ooxml/AGENTS.md) + per-format docs. |
| `src/odr/internal/oldms/` | **Legacy MS binary** (.doc/.ppt/.xls). |
| `src/odr/internal/pdf/` | PDF (own parser). |
| `src/odr/internal/{csv,json,text,svm}/` | Smaller formats. |
| `cli/src/` | CLI tools: `translate`, `back_translate`, `meta`, `server`. |
| `python/` | Python bindings (`pyodr`, pybind11); see [`python/AGENTS.md`](python/AGENTS.md). |
| `jni/` | JNI bindings (Java package `app.opendocument.core`); see [`jni/AGENTS.md`](jni/AGENTS.md). |
| `android/` | The bindings packaged as an AAR (`odr-core-android`) + the instrumented tests; see [`android/AGENTS.md`](android/AGENTS.md). |
| `tools/pdf/` | Dev tooling (not built): PDF encoding-data generators, see `tools/pdf/README.md`. |
| `test/src/` | GoogleTest suites; data in `test/data` (git submodules). |
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
- CMake options (`CMakeLists.txt`): `ODR_TEST`, `ODR_CLI`, `ODR_WITH_LIBMAGIC`,
  `ODR_PYTHON`, `ODR_CLANG_TIDY`. A new `.cpp` must be added to
  `ODR_SOURCE_FILES`.
- **Test data is fetched, not vendored**, and opt in: `-DODR_TEST_FETCH_DATA=ON`
  makes `cmake/test_data.cmake` clone the repositories pinned in
  `test/data.cmake` into `test/data/` (they were submodules until then; read
  that file's header for why). Off by default — building `odr_test` does not
  need the data, and several gigabytes should not arrive because someone turned
  tests on. The `update_test_data` target moves existing checkouts onto the
  pins. The two private repositories need credentials.

## Releasing

Branch `release/v<major>.<minor>.X` off main, push it, publish the draft that
appears. That is the whole procedure — `.github/workflows/release.yml` and
`scripts/release.py`.

- **`main` carries no version.** No file is bumped, no changelog is committed,
  and `git log main` never mentions a release. All a build records about itself
  is `GIT_HEAD_SHA1` plus a dirty flag, and that is deliberate: the version of a
  release is derived from its commits and the tag is where it lives.
- **The version comes from the commit subjects** — `git cliff --bumped-version`
  over the conventional commits since the last *reachable* tag, so `feat!` is a
  major, `feat` a minor and the rest a patch. Writing them properly is therefore
  load-bearing, not cosmetic.
- **Maintenance releases fall out of reachability.** On `release/v6.1.X` the
  last reachable tag is `v6.1.0`, so a cherry-picked `fix:` becomes `v6.1.1`
  while `release/v6.2.X` independently moves to `v6.2.1`.
- **Release branches run `release.yml` and nothing else** — every other workflow
  carries `branches-ignore: ['release/**']`. A merge from main brings commits
  that were already green; a backport that did not come that way is worth
  pushing to an ordinary branch first.
- **The release is drafted, and a human publishes it.** GitHub does not create
  the tag until then, which is what lets the tag point at a commit made during
  the run. It also has to be a human: a release created by `GITHUB_TOKEN` raises
  no `release: published`, and that event is what starts conan (`conan.yml`),
  maven and android.
- **Anything a release must record about itself is written in `release.yml`, in
  one place, and committed by `release.py stamp` as `chore(release): vX.Y.Z`.**
  Nothing else in the repo writes a version anywhere. If a run writes nothing
  there is no commit at all and the tag lands on the merge from main — the
  shape a release should have when nothing forces otherwise. What forces
  otherwise is SwiftPM: it resolves `Package.swift` at the tag, and a binary
  target there names a URL and a sha256 of an archive that does not exist until
  it is built.
- **Publishing fans out, and `release-status.yml` is what makes a partial
  release loud.** Each destination keeps its own workflow, credentials and
  failure modes so one can be re-run without the others; `scripts/release_status.py`
  waits for all of them and writes the outcome into the release body, failing if
  a destination failed *or never started*. Its `EXPECTED` table is the only
  complete list of where a release goes — add a line when a destination is
  added. Release *assets* are not part of that: `release.yml` attaches
  everything the run produced to the draft before it is published.
- `release.py` and `release_status.py` are runnable by hand and `--dry-run`
  mutates nothing.

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
  line **at the top** of the PR body.

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
