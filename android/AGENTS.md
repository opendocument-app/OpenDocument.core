# AGENTS.md — the android AAR

Packages the JNI bindings (`../jni`) as `app.opendocument:odr-core-android` and
is where android specific behaviour is tested. Read [`../jni/AGENTS.md`](../jni/AGENTS.md)
first — the java API and its android constraints live there. User facing docs:
[`README.md`](README.md).

## Layout

| Path | What |
|------|------|
| `build.gradle.kts` | The library module: sources from `../jni/java`, prebuilt native libs and assets, lint, publishing. Single project — `rootProject.name` *is* the artifactId. |
| `build_native.py` | conan + cmake per ABI → `native/prebuilt/{jniLibs,assets}`. Invoked by the `buildNative` gradle task and directly by CI. |
| `src/main/java/.../android/OdrAndroid.java` | The only android specific production code: extracts the bundled assets and registers them with `GlobalParams`. |
| `src/androidTest/` | Instrumented suite, JUnit 4 + androidx.test, inputs from `../jni/testfixtures`. |
| `consumer-rules.pro` | Keeps `app.opendocument.core.**` — JNI resolves it by name, R8 cannot see that. |

## Why it exists

Two android-only failure modes had shipped before this: a `jni/src` compile
error only the NDK sees (#628), and java APIs that exist on the JDK but not on
android's class library (#621). Both are cheap to catch and impossible to catch
host side, so the workflow does:

1. **cross compile every ABI** — covers the first outright;
2. **lint `NewApi` against minSdk 26** over `../jni/java` — covers android.\* and
   the java.\* APIs core library desugaring cannot backport (`Cleaner`), but
   *not* the desugarable ones (`List.of`, `Optional.isEmpty`), so it is a filter,
   not a proof;
3. **the instrumented suite on an API 26 emulator** — the only check that sees
   what a device sees, and the one that settles the rest.

A new binding is therefore only covered once something in `src/androidTest`
calls it.

## Rules

- **Do not duplicate the java API here.** `main` compiles `../jni/java`
  verbatim; the AAR and the maven jar are the same classes, and anything
  android-only goes into the `app.opendocument.core.android` package.
- **Shared test inputs stay in `../jni/testfixtures`**, which both suites
  compile — so it is limited to what android API 26 offers (no `Path.of`, no
  `Files.writeString`, no `String.formatted`).
- **`native/` is build output**, never committed; the artifacts CI downloads
  land in the same place. `-Podr.abis=` (empty) is what tells the build the
  libraries are already there.
- Keep AGP and gradle in step with OpenDocument.droid — it is the consumer that
  finds the incompatibilities first.
- The manifest stays permission-free; permissions the server needs are the
  app's call, and documented in `README.md` instead.
- Assets keep the `core/odrcore` + `core/libmagic` layout of the droid conan
  deployer, so an app can move between the two packagings without touching its
  extraction code.
