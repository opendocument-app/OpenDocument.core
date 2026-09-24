# odr-core-java — JNI bindings for OpenDocument.core

Decode office documents (ODF, OOXML, legacy MS binary, PDF, CSV, ...) and
render them to HTML from Java (package `app.opendocument.core`).

```java
import app.opendocument.core.DecodedFile;
import app.opendocument.core.Html;
import app.opendocument.core.HtmlConfig;
import app.opendocument.core.HtmlService;
import app.opendocument.core.Odr;

DecodedFile file = Odr.open("document.odt");
HtmlService service = Html.translate(file, new HtmlConfig());
app.opendocument.core.Html html = service.bringOffline("output-dir");
for (var page : html.pages()) {
    System.out.println(page.name + " " + page.path);
}
```

The native library loads from `java.library.path` as `odr_jni`
(`libodr_jni.so` or `libodr_jni.dylib`). The system property
`app.opendocument.core.library` overrides it with an absolute path.

## Maven distribution

The Java classes are published as `app.opendocument:odr-core-java` to Maven
Central and to
[GitHub Packages](https://github.com/orgs/opendocument-app/packages?repo_name=OpenDocument.core)
on release (`.github/workflows/maven.yml`, from `pom.xml`). The artifact
contains only the Java API. A consumer builds the native `odr_jni` library for
its platform (see below) and provides it at runtime.

On android use `app.opendocument:odr-core-android` instead. It is an AAR with
the same classes plus the native library for every ABI. See
[`../android`](../android/README.md).

```gradle
repositories {
    mavenCentral()
}

dependencies {
    implementation "app.opendocument:odr-core-java:<version>"
}
```

Prefer Central. GitHub Packages needs a token with `read:packages` even for a
public package.

Local build: `mvn --file jni/pom.xml verify` produces the jar plus the sources
and javadoc jars in `jni/target/`. The `central` profile (`mvn deploy
-Pcentral`) adds the PGP signatures and the `developers` POM entry that Central
requires. It is off the default build, so neither a signing key nor a portal
token is needed to build or to deploy to GitHub Packages.

## Building

The bindings are part of the main CMake build, toggled by `ODR_JNI`. The jar
needs a JDK 17 or newer (`--release 17`):

```bash
conan install . -o '&:with_jni=True' --build missing
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DODR_JNI=ON -DODR_TEST=ON
cmake --build build --target odr_jni odr_java odr_java_tests
(cd build/jni && ctest)
```

This produces `build/jni/libodr_jni.dylib` (or `.so`) and
`build/jni/odr-core-java.jar`. `jni/CMakeLists.txt` also configures standalone
against an installed `odrcore` package.

`ODR_JNI_JAR=OFF` builds the native library alone. The AAR build
(`android/build_native.py`) uses it, because it compiles `jni/java/` with the
android toolchain. It is the only build that needs no JDK. Otherwise a missing
JDK fails the configure step.

## Runtime data

There is none. The renderer's CSS and JS are part of the library, and
detection needs no database.

## Notes

- Handle-backed objects (`DecodedFile`, `Document`, `Element`,
  `HtmlService`, ...) own native memory. They free it on garbage collection.
  Use `close()` or try-with-resources to release a large object early.
- The C++ `HtmlConfig::resource_locator` callback is not exposed. The standard
  resource locator is always used.
- `HttpServer` is available when the native library was built with the HTTP
  server (`Odr.hasHttpServer()`).
