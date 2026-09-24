# AGENTS.md — JNI bindings

Hand-written JNI bindings for the public C++ API (`src/odr/*.hpp`), Java
package `app.opendocument.core`. They mirror the surface of the python
bindings (`python/`).

## Layout

| Path | What |
|------|------|
| `CMakeLists.txt` | Builds `libodr_jni` and `odr-core-java.jar`. Included from the root build via `ODR_JNI`, or standalone against an installed `odrcore`. `ODR_JNI_JAR=OFF` builds the native half alone, which is what `../android` does and the only build that needs no JDK. |
| `pom.xml` | Maven distribution of the Java classes (`app.opendocument:odr-core-java`). `.github/workflows/maven.yml` publishes it to Maven Central (profile `central`) and GitHub Packages (profile `github`) on release. Keep `--release` and `-Xlint` in sync with `CMAKE_JAVA_COMPILE_FLAGS`. |
| `src/` | One `jni_*` unit per public-API area. `odr_jni.hpp` (strings, exceptions, handles) and `jni_convert.hpp` (struct and POJO marshalling) are the helpers. |
| `java/app/opendocument/core/` | The Java API: enums, POJOs (styles, metas, `HtmlConfig`), and handle-backed wrappers that extend `NativeResource`. `../android` compiles the same sources into the AAR. It stays java, because `add_jar` has no kotlin toolchain. |
| `tests/` | JUnit 5 suite, run through ctest (`odr_jni_junit`). Inputs come from `TestFiles`. |
| `testfixtures/` | `TestFiles`, shared with the instrumented suite of the AAR, so it uses only what android API 26 offers. `resources/` holds the one document. `add_jar`'s `RESOURCES NAMESPACE` packs it into the test jar and `../android/build.gradle.kts` into the test apk. |

## Design

- Handle model: a Java wrapper owns a heap-allocated copy of the C++ value
  handle (`odr::DecodedFile`, `odr::Element`, ...) as a `long`.
  `NativeResource` frees it through a `PhantomReference` reaper thread and
  `AutoCloseable`. Each class has a static `destroy(long)` native that deletes
  the concrete C++ type.
- Typed views are re-derived per call. A handle always points to the base
  type (`odr::Element`, `odr::DecodedFile`). A native of a typed Java class
  calls `as_paragraph()`, `as_text_file()` and so on each time. Never store a
  typed C++ subobject behind a base-typed handle.
- Keep-alive: a navigation result carries an `owner` reference
  (`Element.owner()` is the `Document`), so the GC cannot free the root while
  a handle into it is alive.
- GC safety: a native that uses a handle is an instance method of the object
  that owns it. The `this` reference keeps the wrapper and its owner chain
  reachable during the call. Only factories and `destroy` are static. A static
  native that takes another object's handle is a bug even where it reads fine:
  once `handle()` returns, nothing refers to the wrapper, so the reaper may
  free the handle during the call.
- A handle in argument position, `a.fooNative(handle(), b.handle())`, has no
  receiver that holds it. So `b` needs `NativeResource.keepAlive()` in a
  `finally` after the call. `Html.translate` does this. `keepAlive` stands in
  for `Reference.reachabilityFence`, which android has only from API 28.
- Enums cross as ordinals. The Java constant order must match the C++ enum
  declaration. `-1` encodes an absent `std::optional`.
- Strings go through `odr_jni::to_string` and `to_jstring` (real UTF-8 and
  UTF-16). Never use JNI's modified-UTF-8 `GetStringUTFChars`.
- Exceptions: every native body runs inside `odr_jni::guarded`. `throw_java`
  names the `OdrException` subclass after `odr::ErrorCode`. A code with no
  class arrives as the base, so the mapping cannot drift. Every exception
  carries `getCode()`.
- Mirror the C++ names. `Logger` is a `NativeResource`. An entry point that
  takes one gets an overload, for example `Odr.open(path, logger)`.
- `ILogger` is implementable in Java. `jni_logger.cpp`'s `JavaLogger` holds a
  global ref to the sink and routes calls through the package-private
  `LoggerBridge` statics, so only three method handles are cached. A log call
  arrives on the thread the library works on, so `ScopedEnv` attaches and
  detaches it. An exception from the sink is described and cleared, because a
  logger must not derail the operation it reports on.
- Stream-based C++ APIs (`write`, `save`, `pipe`) return `byte[]` or
  `String` through `std::ostringstream`.
- Not bound: `HtmlConfig::resource_locator` (a function pointer across JNI).
  The standard resource locator is always used. A `null`
  `HtmlConfig.resourcePath` keeps the C++ default, so do not marshal it
  unconditionally.
- New public C++ API: extend the matching `jni_*.cpp` and Java class, and add
  a JUnit test.
- Android API level 26 is the java floor. OpenDocument.droid consumes this
  artifact with `minSdk = 26`, and android ships an older `java.*` than the
  `--release 17` compiler accepts. Anything newer fails at runtime, on device
  only, with `NoClassDefFoundError` or `NoSuchMethodError`. Known traps:
  `java.lang.ref.Cleaner` (API 33, hence the `PhantomReference` reaper),
  `Reference.reachabilityFence` (API 28, hence `keepAlive`), `List.of`,
  `Set.of`, `Map.of` (API 34), `Optional.isEmpty` (API 33), `java.time` (only
  in part on API 26). Core library desugaring does not cover `java.lang.ref`,
  and an app cannot shim a `java.*` class, so the fix has to happen here.
  `../android` cross compiles the bindings, lints `java/` against minSdk 26
  and runs the instrumented suite on an API 26 emulator on every push.
- C++ follows the repo clang-format. Java follows google-java-format style
  (2-space indent). No formatter is enforced.
- Every test input comes from `testfixtures/.../TestFiles.java`. Nothing
  reads `test/data/`, because an android build tree does not have it. The
  document is one 9 KB file from OpenDocument.test. Text and CSV stay inline.
  Do not build the test ODT in the test itself, because that only proves the
  library reads back what the test wrote.
- Use `127.0.0.1`, not `localhost`, for the HTTP server. The JVM prefers `::1`.
- Build and test loop: [`README.md`](README.md). CI builds the bindings in the
  `build` job of `.github/workflows/build_test.yml` (host) and in
  `.github/workflows/android.yml` (android).
