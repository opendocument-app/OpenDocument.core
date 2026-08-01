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
| `src/main/java/.../android/OdrAndroid.kt` | The only android specific production code: extracts the bundled assets and registers them with `GlobalParams`. |
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
- **What this module writes is kotlin; what it borrows is java.** `../jni/java`
  and `../jni/testfixtures` are compiled by CMake's `add_jar` for the maven jar
  and the host junit suite, which have no kotlin toolchain, so they stay java —
  the kotlin here is only `OdrAndroid` and `src/androidTest`. Anything crossing
  back to a java caller keeps its java shape: `@JvmStatic` so `OdrAndroid.init`
  stays a static call, `@Throws` so the `IOException` stays checked.
- **Formatting is ktfmt** (kotlinlang style) via spotless, the same version
  OpenDocument.droid runs. `./gradlew spotlessApply`; CI checks it in
  `.github/workflows/format.yml`, which needs neither the NDK nor conan.
- **Shared test inputs stay in `../jni/testfixtures`**, which both suites
  compile — so it is limited to what android API 26 offers (no `Path.of`, no
  `Files.writeString`, no `String.formatted`). Its `resources/` are on the
  androidTest source set as **java resources**, not assets: `TestFiles` reads
  the document off the classpath, and an asset would only be reachable from the
  android half of the suite.
- **Publishing goes to Maven Central and GitHub Packages**, via
  `com.vanniktech.maven.publish` — sonatype ships no official gradle plugin for
  the portal. Central's extra requirements (sources + javadoc jars, `developers`
  in the POM, a PGP signature per file) drive what that block declares; do not
  drop one thinking GitHub Packages is the only consumer. Signing is conditional
  on a key being present so `publishToMavenLocal` works without one.
- **`native/` is build output**, never committed; the artifacts CI downloads
  land in the same place. `-Podr.abis=` (empty) is what tells the build the
  libraries are already there.
- Keep AGP and gradle in step with OpenDocument.droid — it is the consumer that
  finds the incompatibilities first.
- The manifest stays permission-free; permissions the server needs are the
  app's call, and documented in `README.md` instead.
- Assets keep the `core/odrcore` layout of the droid conan deployer, so an app
  can move between the two packagings without touching its extraction code.
