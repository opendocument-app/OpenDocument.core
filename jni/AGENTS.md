# AGENTS.md — JNI bindings

Hand-written JNI bindings for the public C++ API (`src/odr/*.hpp`), Java
package `app.opendocument.core`. Mirrors the surface of the python bindings
(`python/`); replaces the ad-hoc odr.droid `CoreWrapper`.

## Layout

| Path | What |
|------|------|
| `CMakeLists.txt` | Builds `libodr_jni` + `odr-core-java.jar`; included from the root build via `ODR_JNI`, or standalone against an installed `odrcore`. |
| `src/` | JNI sources, one `jni_*` unit per public-API area; `odr_jni.hpp` (strings, exceptions, handles) and `jni_convert.hpp` (struct/POJO marshalling) are the helpers. |
| `java/app/opendocument/core/` | Java API: enums, POJOs (styles, metas, `HtmlConfig`), and handle-backed wrappers extending `NativeResource`. |
| `tests/` | JUnit 5 suite, run via ctest (`odr_jni_junit`); inputs are generated inline (tmp files, zip-built minimal ODT) — no fixture files. |

## Design

- **Handle model**: a Java wrapper owns a heap-allocated copy of the C++ value
  handle (`odr::DecodedFile`, `odr::Element`, ...) referenced by a `long`.
  `NativeResource` frees it via `Cleaner`/`AutoCloseable`; each class has a
  static `destroy(long)` native deleting the concrete C++ type.
- **Typed views are re-derived per call**: element/file handles always point to
  the *base* type (`odr::Element`, `odr::DecodedFile`); natives of typed Java
  classes call `as_paragraph()`/`as_text_file()` etc. on each call. Never store
  a typed C++ subobject behind a base-typed handle (slicing/delete issues).
- **Keep-alive**: navigation results carry an `owner` reference
  (`Element.owner()` → the `Document`) so the GC cannot free the root while
  handles into it are alive — the JNI analogue of pyodr's `keep_alive`.
- **GC safety**: natives that use a handle are *instance* methods (the `this`
  local reference keeps the wrapper — and its owner chain — reachable for the
  duration of the call); only factories and `destroy` are static.
- **Enums cross as ordinals**: Java enum constant order must match the C++
  enum declaration; `-1` encodes an absent `std::optional`.
- **Strings**: use `odr_jni::to_string`/`to_jstring` (real UTF-8 ↔ UTF-16),
  never JNI's modified-UTF-8 `GetStringUTFChars`.
- **Exceptions**: every native body runs inside `odr_jni::guarded`; C++
  exceptions map to `OdrException` subclasses (`odr_jni.cpp::throw_java`).
- Mirror the C++ names; drop the `Logger` parameters (bindings use the default
  null logger).
- Stream-based C++ APIs (`write`, `save`, `pipe`) are bound as natives
  returning `byte[]`/`String` via `std::ostringstream`.
- **Not bound**: `HtmlConfig::resource_locator` (function pointer across JNI);
  the standard resource locator is always used. A `null` Java
  `HtmlConfig.resourcePath` keeps the C++ default (the odr core data path) —
  don't marshal it unconditionally.
- New public C++ API? Extend the matching `jni_*.cpp` + Java class and add a
  JUnit test.
- C++ sources follow the repo clang-format; Java follows the
  google-java-format style (2-space indent), no enforced formatter yet.
- Tests must stay hermetic: build inputs inline in `tests/.../TestFiles.java`;
  HTML-rendering tests `assumeTrue(TestFiles.hasCoreData())` (skips when
  assets are missing). Use `127.0.0.1`, not `localhost`, for the HTTP server
  (the JVM prefers `::1`).
- Build/test loop: see `jni/README.md`; CI lives in
  `.github/workflows/jni.yml`.
