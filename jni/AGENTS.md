# AGENTS.md — JNI bindings

Hand-written JNI bindings for the public C++ API (`src/odr/*.hpp`), Java
package `app.opendocument.core`. Mirrors the surface of the python bindings
(`python/`); replaces the ad-hoc odr.droid `CoreWrapper`.

## Layout

| Path | What |
|------|------|
| `CMakeLists.txt` | Builds `libodr_jni` + `odr-core-java.jar`; included from the root build via `ODR_JNI`, or standalone against an installed `odrcore`. |
| `pom.xml` | Maven distribution of the Java classes only (`app.opendocument:odr-core-java`); published to Maven Central (the `central` profile) and GitHub Packages on release via `.github/workflows/maven.yml`. Keep `--release`/`-Xlint` in sync with `CMAKE_JAVA_COMPILE_FLAGS`. |
| `src/` | JNI sources, one `jni_*` unit per public-API area; `odr_jni.hpp` (strings, exceptions, handles) and `jni_convert.hpp` (struct/POJO marshalling) are the helpers. |
| `java/app/opendocument/core/` | Java API: enums, POJOs (styles, metas, `HtmlConfig`), and handle-backed wrappers extending `NativeResource`. Also compiled as-is into the AAR (`../android`) — which is kotlin, but this stays java: `add_jar` below has no kotlin toolchain. |
| `tests/` | JUnit 5 suite, run via ctest (`odr_jni_junit`); inputs come from `TestFiles`. |
| `testfixtures/` | `TestFiles`, shared with the instrumented suite of the AAR — hence limited to what android API 26 offers. `resources/` holds the one document, packaged into the test jar by `add_jar`'s `RESOURCES NAMESPACE` and into the test apk by `../android/build.gradle.kts`. |

## Design

- **Handle model**: a Java wrapper owns a heap-allocated copy of the C++ value
  handle (`odr::DecodedFile`, `odr::Element`, ...) referenced by a `long`.
  `NativeResource` frees it via a `PhantomReference` reaper thread and
  `AutoCloseable`; each class has a static `destroy(long)` native deleting the
  concrete C++ type.
- **Typed views are re-derived per call**: element/file handles always point to
  the *base* type (`odr::Element`, `odr::DecodedFile`); natives of typed Java
  classes call `as_paragraph()`/`as_text_file()` etc. on each call. Never store
  a typed C++ subobject behind a base-typed handle (slicing/delete issues).
- **Keep-alive**: navigation results carry an `owner` reference
  (`Element.owner()` → the `Document`) so the GC cannot free the root while
  handles into it are alive — the JNI analogue of pyodr's `keep_alive`.
- **GC safety**: natives that use a handle are *instance* methods **of the
  object that owns it** (the `this` local reference keeps the wrapper — and its
  owner chain — reachable for the duration of the call); only factories and
  `destroy` are static. A static native taking someone else's handle is a bug
  even where it reads fine: once `handle()` has returned nothing refers to the
  wrapper, so the collector may enqueue it and the reaper free the handle while
  the call is still running. Put the native on the owner and let the entry point
  delegate — `Html.edit` → `Document.edit`, `new DecodedFile(file)` →
  `File.decode`, and `Logger`'s own natives.
- **Handles in argument position** — `a.fooNative(handle(), b.handle())` — have
  no receiver holding them, so `b` needs `NativeResource.keepAlive()` in a
  `finally` after the call. That is what `Reference.reachabilityFence` is for;
  android only has it from API 28 (see the API floor below), so `keepAlive` is
  the JDK's own pre-intrinsic fallback instead.
- **Enums cross as ordinals**: Java enum constant order must match the C++
  enum declaration; `-1` encodes an absent `std::optional`.
- **Strings**: use `odr_jni::to_string`/`to_jstring` (real UTF-8 ↔ UTF-16),
  never JNI's modified-UTF-8 `GetStringUTFChars`.
- **Exceptions**: every native body runs inside `odr_jni::guarded`; C++
  exceptions map to `OdrException` subclasses (`odr_jni.cpp::throw_java`).
- Mirror the C++ names. `Logger` is bound as a `NativeResource`; entry points
  that take one get an overload (e.g. `Odr.open(path, logger)`).
- `ILogger` is implementable in Java. `jni_logger.cpp`'s `JavaLogger` holds a
  global ref to the sink and routes calls through the package-private
  `LoggerBridge` statics, so only three method handles need caching. Log calls
  arrive on whatever thread the library works on, so it attaches via `ScopedEnv`
  and detaches again; an exception thrown by the sink is described and cleared
  rather than left pending, since a logger must not derail the operation it
  reports on.
- Stream-based C++ APIs (`write`, `save`, `pipe`) are bound as natives
  returning `byte[]`/`String` via `std::ostringstream`.
- **Not bound**: `HtmlConfig::resource_locator` (function pointer across JNI);
  the standard resource locator is always used. A `null` Java
  `HtmlConfig.resourcePath` keeps the C++ default (the odr core data path) —
  don't marshal it unconditionally.
- New public C++ API? Extend the matching `jni_*.cpp` + Java class and add a
  JUnit test.
- **Android API level 26 is the java floor**: OpenDocument.droid consumes this
  artifact with `minSdk = 26`, and android ships a much older `java.*` than the
  `--release 17` compiler accepts. Anything newer fails at runtime, on device
  only, with `NoClassDefFoundError`/`NoSuchMethodError`. Known traps, all of
  which android added far later than the JDK: `java.lang.ref.Cleaner` (API 33,
  hence the `PhantomReference` reaper),
  `java.lang.ref.Reference.reachabilityFence` (API 28, hence `keepAlive`),
  `List.of`/`Set.of`/`Map.of` (API 34),
  `Optional.isEmpty` (API 33), `java.time` (API 26 only in part). Core library
  desugaring does not cover `java.lang.ref`, and an app cannot shim a `java.*`
  class, so the fix always has to happen here. This is no longer only a rule to
  remember: `../android` cross compiles the bindings, lints `java/` against
  minSdk 26 and runs an instrumented suite on an API 26 emulator on every push.
- C++ sources follow the repo clang-format; Java follows the
  google-java-format style (2-space indent), no enforced formatter yet.
- Every input comes from `testfixtures/.../TestFiles.java` and nothing reaches
  for `test/data/` — an android build tree does not have it, and a fetch at test
  time is not a dependency a test should carry. The document is one 9 KB file
  from OpenDocument.test carried alongside; text and CSV need no container and
  stay inline. Do not go back to building a zip here: the ODT this suite ran on
  for a while was written by `TestFiles` itself, which proved only that odrcore
  could read back what the test had written.
  HTML-rendering tests `assumeTrue(TestFiles.hasCoreData())` (skips when
  assets are missing). Use `127.0.0.1`, not `localhost`, for the HTTP server
  (the JVM prefers `::1`).
- Build/test loop: see `jni/README.md`; CI builds the bindings in the `build`
  job of `.github/workflows/build_test.yml` (host) and in
  `.github/workflows/android.yml` (android).
