# AGENTS.md — the android AAR

Packages the JNI bindings (`../jni`) as `app.opendocument:odr-core-android`
and tests android-specific behaviour. Read [`../jni/AGENTS.md`](../jni/AGENTS.md)
first: the java API and its android constraints live there. User-facing docs:
[`README.md`](README.md).

## Layout

| Path | What |
|------|------|
| `build.gradle.kts` | The library module: sources from `../jni/java`, prebuilt native libs, lint, publishing. A single project; `rootProject.name` is the artifactId. |
| `build_native.py` | conan plus cmake per ABI into `native/prebuilt/jniLibs`. The `buildNative` gradle task and CI run it. |
| `src/androidTest/` | Instrumented suite, JUnit 4 plus androidx.test. Inputs from `../jni/testfixtures`. |
| `consumer-rules.pro` | Keeps `app.opendocument.core.**`. JNI resolves the classes by name, and R8 cannot see that. |

## What the module checks

Two failure modes exist only on android: a `jni/src` compile error that only
the NDK sees, and a `java.*` API that the JDK has but android's class library
does not. The workflow catches both:

1. Cross compile every ABI.
2. Lint `NewApi` against minSdk 26 over `../jni/java`. This covers
   `android.*` and the `java.*` APIs that core library desugaring cannot
   backport (`Cleaner`), but not the desugarable ones (`List.of`,
   `Optional.isEmpty`). It is a filter, not a proof.
3. Run the instrumented suite on an API 26 emulator. This is the only check
   that sees what a device sees.

A new binding is covered only once something in `src/androidTest` calls it.

## Rules

- Do not duplicate the java API here. `main` compiles `../jni/java` verbatim,
  so the AAR and the maven jar hold the same classes. Anything android-only
  goes into the `app.opendocument.core.android` package.
- What this module writes is kotlin, what it borrows is java. `../jni/java`
  and `../jni/testfixtures` stay java, because CMake's `add_jar` has no kotlin
  toolchain. The kotlin here is only `src/androidTest`. The module ships no
  production code of its own. Anything that a java caller uses keeps its java
  shape: `@JvmStatic` for a static call, `@Throws` for a checked exception.
- Formatting is ktfmt (kotlinlang style) through spotless, the same version
  OpenDocument.droid runs. `./gradlew spotlessApply` fixes it.
  `.github/workflows/format.yml` checks it without the NDK or conan.
- Shared test inputs stay in `../jni/testfixtures`, which both suites
  compile, so it uses only what android API 26 offers (no `Path.of`, no
  `Files.writeString`, no `String.formatted`). Its `resources/` sit on the
  androidTest source set as java resources, not assets, because `TestFiles`
  reads the document off the classpath.
- Publishing goes to Maven Central and GitHub Packages through
  `com.vanniktech.maven.publish`. Central requires sources and javadoc jars,
  `developers` in the POM, and a PGP signature per file. Do not drop one of
  them. Signing is conditional on `signingInMemoryKey`, so
  `publishToMavenLocal` works without a key.
- `native/` is build output and is never committed. The artifacts CI downloads
  land in the same place. `-Podr.abis=` (empty) tells the build the libraries
  are already there.
- Keep AGP and gradle in step with OpenDocument.droid. It is the consumer that
  finds an incompatibility first.
- The manifest declares no permission. The permissions the server needs are
  the app's call, and `README.md` documents them.
