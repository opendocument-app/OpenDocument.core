# AGENTS.md — the WebAssembly bindings

Embind bindings for the public C++ API (`src/odr/*.hpp`), packaged as the npm
package `@opendocument/odr-core`. The browser counterpart of
[`../python`](../python/AGENTS.md); read that first, the layout convention is
the same.

The point is a viewer that renders client-side with no upload and no backend.
The library was already in the right shape: every renderer writes to a
`std::ostream`, the CSS and JS are string literals compiled in
(`internal/html/frontend.cpp`), and with `HtmlConfig::embed_images` one view is
a complete HTML document whose emitted JS is pure DOM — no `fetch`, no `XHR`.

## Layout

| Path | What |
|------|------|
| `CMakeLists.txt` | The `odr_wasm` target, behind `ODR_WASM`. Fails fast without Emscripten, and on `BUILD_SHARED_LIBS`. |
| `src/` | The bindings, one unit per public-API area; `odr_wasm.{hpp,cpp}` holds the session registry, the result envelope and the exception mapping. |
| `js/` | The hand-written half of the package. Copied next to the generated glue at build time, so the build directory is importable and `npm pack` has one source. |
| `tests/` | `node --test` suite, run via ctest (`odr_wasm_node`). |
| `testfixtures/` | The two documents the suite cannot build in memory. |
| `example/` | A no-bundler page for eyeballing output. Not packaged, but `wasm.yml` repoints its import and ships it in the release zip, so keep that import a plain relative path. |

## The three rules, and why they are not the other bindings' rules

Everything unusual here follows from the binding being driven from a **Web
Worker**, where every value that crosses is structured-cloned.

- **Nothing throws across the boundary.** Every entry point runs inside
  `guarded` and returns `{ok, value | error}`. The worker protocol has to turn
  a failure into data regardless, and an *unconverted* C++ exception reaches JS
  as an opaque pointer. `js/index.js` turns the envelope back into a thrown
  `OdrError`, so only the wire carries envelopes. The `error.type` names come
  from the same list as `jni/src/odr_jni.cpp`'s `throw_java` and
  `apple/src/ODRInternal.mm`; keep the three in step.
- **Nothing escapes as an embind handle.** A `class_`-bound wrapper cannot be
  structured-cloned, so a document is a `std::uint32_t` into a registry and a
  view an index within its session. This also dissolves the keep-alive problem
  the other bindings hand-built: `HtmlView` holds a bare pointer into its
  service and `Element` into the document adapter, and here neither is handed
  out — `Session` owns file, document, service and views together. The
  document is the *one* tree the render, the edit and the save all go through:
  `DocumentFile::document()` decodes a fresh one per call, so a `save` that
  opened its own would write the document nobody edited. Handle `0` is never
  issued, so a zeroed handle is always invalid.
- **Config crosses as a plain object.** `to_html_config` reads known keys and
  leaves the rest defaulted. Never bind a mutable config: it could not cross
  `postMessage`.

## Rules

- **Bind the public API only** — never include `odr/internal/...`, with the one
  deliberate exception of `odr_meta_util.hpp`, reused so the meta blob matches
  `cli/src/meta.cpp` byte for byte.
- **An embind `std::string` parameter is binary-safe; a `std::string` return is
  not.** A parameter takes a `Uint8Array` verbatim, a return goes through
  `UTF8ToString` under the default `-sEMBIND_STD_STRING_IS_UTF8`. Fine for HTML,
  wrong for a PNG or a font, so binary results go through `to_uint8_array`.
- **`to_uint8_array` copies, deliberately.** A `typed_memory_view` aliases the
  wasm heap and `ALLOW_MEMORY_GROWTH` detaches it on the next allocation.
- **Enums cross by ordinal.** `enum_tables()` derives `FileType`,
  `FileCategory` and `DocumentType` from the library's own tables; the rest are
  listed by hand in `wasm_core.cpp` and pinned by `tests/enums.test.mjs`.
  Appending stays silent, reordering goes loud — the rule
  `src/odr/html.hpp:34` and `src/odr/file.hpp` state.
- **A C++→JS callback must be worker-local and synchronous.** Both of them —
  the logger sink, and the resource locator when it lands — are called during a
  render, and one needing the main thread would deadlock behind a `postMessage`
  round trip.
- **The package is plain JavaScript with a hand-written `.d.ts`.** A TypeScript
  source tree would drag `tsc` into a C++ repository for one file of
  declarations. Keep `js/index.d.ts` in step with `js/index.js` by hand.
- **Test inputs are built in memory** (`tests/helper.mjs` has a hand-rolled zip
  writer), following `python/AGENTS.md`. `testfixtures/` holds only what cannot
  be: a document with real layout, and an encrypted one. Never `test/data/` —
  that is gigabytes behind two private repositories and opt-in.

## Build

Emscripten only; see [`../README.md`](README.md) for the conan invocation and
`.github/config/conan/profiles/emscripten-wasm` for the profile.

- **No `-pthread`.** It implies SharedArrayBuffer, which implies COOP/COEP
  headers on whoever hosts the viewer, which rules out plain GitHub Pages.
  Outside `http_server.cpp` — excluded here — the library spawns no thread.
- **`-fwasm-exceptions` lives in the profile's `[conf] tools.build:*flags`, not
  in CMake flags.** The EH mode is an ABI: every dependency has to be built
  with the same one or the link fails.
- **`compiler.threads` is omitted, not set to `null`.** A profile value is a
  string, so `null` reads as the literal `"null"` and fails against
  `settings.yml`. Omitting the line is how "unset" is spelled — this looks like
  an oversight and is not.
- **`-sSTACK_SIZE=8388608` in `CMakeLists.txt` is load-bearing.** Emscripten
  defaults to 64 KB and the element-registry builders, the renderer's tree walk
  and the PDF object parser all recurse. The failure at 64 KB does not look
  like a stack overflow.

Every dependency cross-compiles, cryptopp included and unpatched — no
`CRYPTOPP_DISABLE_ASM`. Output is byte-identical to the native build across
odt, docx, ods, xlsx, odp, pptx, doc, xls, ppt, pdf, odg, csv and txt, and for
encrypted docx/ods/odt with their passwords, which covers endianness, float
formatting and hash ordering in one check. At `-O3` and before any size tuning
the whole library is 2.9 M of wasm, 831 K brotli'd, plus 92 K of JS glue — so
splitting PDF into a lazily loaded second bundle is not worth introducing.
`-Oz` and `-flto` are untried.
