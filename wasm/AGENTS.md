# AGENTS.md — the WebAssembly bindings

Embind bindings for the public C++ API (`src/odr/*.hpp`), packaged as the npm
package `@opendocument/odr-core`. The layout convention is the one in
[`../python/AGENTS.md`](../python/AGENTS.md). Read that first.

The goal is a viewer that renders client-side with no upload and no backend.
Every renderer writes to a `std::ostream`, the CSS and JS are compiled in
(`internal/html/frontend.cpp`), and with `HtmlConfig::embed_images` one view
is a complete HTML document whose JS is pure DOM.

## Layout

| Path | What |
|------|------|
| `CMakeLists.txt` | The `odr_wasm` target, behind `ODR_WASM`. Fails without Emscripten and with `BUILD_SHARED_LIBS`. |
| `src/` | The bindings, one unit per public-API area. `odr_wasm.{hpp,cpp}` holds the session registry, the result envelope and the exception mapping. |
| `js/` | The hand-written half of the package: `index.js`, `index.d.ts`, `package.json`. Copied next to the generated glue at build time. |
| `tests/` | `node --test` suite, run by ctest as `odr_wasm_node`. |
| `testfixtures/` | The two documents the suite cannot build in memory. |
| `example/` | A no-bundler page. `wasm.yml` repoints its import and ships it in the release zip, so keep that import a plain relative path. |

## The worker rules

The binding is driven from a Web Worker, and every value that crosses is
structured-cloned. Three rules follow.

- Nothing throws across the boundary. Every entry point runs inside `guarded`
  and returns `{ok, value | error}`. `js/index.js` turns the envelope back into
  a thrown `OdrError`. `error.type` and `error.code` come from
  `odr::ErrorCode`.
- Nothing escapes as an embind handle, because a `class_`-bound wrapper cannot
  be structured-cloned. A document is a `std::uint32_t` into a registry and a
  view an index within its session. `Session` owns file, document, service
  and views together, so the bare pointers in `HtmlView` and `Element` never
  leave C++. The document is the one tree that render, edit and save share,
  because `DocumentFile::document()` decodes a fresh one per call. A plain
  text file keeps its edit in `TextFile::edit`, and the same calls answer for
  it. Handle `0` is never issued. Structural edits address an element by id,
  the number the render writes as `data-odr-id`, so there is no element
  surface at all.
- Config crosses as a plain object. `to_html_config` reads known keys and
  leaves the rest defaulted. Never bind a mutable config.

## Rules

- Bind the public API only. Never include `odr/internal/...`, except
  `odr_meta_util.hpp`, so the meta blob matches `cli/src/meta.cpp` byte for
  byte.
- An embind `std::string` parameter is binary-safe, a `std::string` return is
  not: it goes through `UTF8ToString`. Binary results go through
  `to_uint8_array`.
- `to_uint8_array` copies. A `typed_memory_view` aliases the wasm heap, and
  `ALLOW_MEMORY_GROWTH` detaches it on the next allocation.
- Enums cross by ordinal. `enum_tables()` in `wasm_core.cpp` derives
  `ErrorCode`, `FileType`, `FileCategory` and `DocumentType` from the
  library's tables. The rest are listed by hand there and pinned by
  `tests/enums.test.mjs`. Append to an enum, never reorder it.
- A C++ to JS callback must be worker-local and synchronous. The logger sink
  is called during a render, and one that needs the main thread deadlocks
  behind a `postMessage` round trip. The same holds for a future resource
  locator.
- The package is plain JavaScript with a hand-written `js/index.d.ts`. Keep it
  in step with `js/index.js` by hand.
- Test inputs are built in memory. `tests/helper.mjs` has a zip writer.
  `testfixtures/` holds only a document with real layout and an encrypted one.
  Never use `test/data/`.

## Build

Emscripten only. See [`README.md`](README.md) for the conan invocation and
`.github/config/conan/profiles/emscripten-wasm` for the profile.

- No `-pthread`. It implies SharedArrayBuffer, which implies COOP/COEP headers
  on the host, which rules out plain GitHub Pages. Without `http_server.cpp`
  the library spawns no thread.
- `-fwasm-exceptions` lives in the profile's `[conf] tools.build:*flags`, not
  in CMake flags. The EH mode is an ABI, so every dependency needs the same
  one.
- `compiler.threads` is omitted from the profile, not set to `null`. A profile
  value is a string, so `null` fails against `settings.yml`.
- `-sSTACK_SIZE=8388608` in `CMakeLists.txt` is required. Emscripten defaults
  to 64 KB, and the registry builders, the renderer's tree walk and the PDF
  object parser all recurse. The failure at 64 KB does not look like a stack
  overflow.

Every dependency cross-compiles unpatched, cryptopp included. Output is
byte-identical to the native build across every format, including encrypted
docx, ods and odt. The whole library is about 2.9 MB of wasm, about 830 KB
with brotli, so splitting PDF into a lazily loaded bundle is not worth it.
`-Oz` and `-flto` are untried.
