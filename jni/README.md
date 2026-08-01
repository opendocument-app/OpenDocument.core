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
HtmlService service = Html.translate(file, "cache-dir", new HtmlConfig());
app.opendocument.core.Html html = service.bringOffline("output-dir");
for (var page : html.pages()) {
    System.out.println(page.name + " " + page.path);
}
```

The native library is loaded from `java.library.path` as `odr_jni`
(`libodr_jni.so`/`libodr_jni.dylib`); the system property
`app.opendocument.core.library` overrides it with an absolute path.

## Maven distribution

The Java classes are published as `app.opendocument:odr-core-java` to Maven
Central and to
[GitHub Packages](https://github.com/orgs/opendocument-app/packages?repo_name=OpenDocument.core)
on release (`.github/workflows/maven.yml`, built from `pom.xml`). The artifact
contains **only the Java API** — consumers build the native `odr_jni` library
themselves for their target platform (see below) and provide it at runtime.

On android there is a second, self-contained artifact:
`app.opendocument:odr-core-android`, an AAR with these same classes plus the
native library for every ABI. See [`../android`](../android/README.md).

```gradle
repositories {
    mavenCentral()
}

dependencies {
    implementation "app.opendocument:odr-core-java:<version>"
}
```

Prefer Central: GitHub Packages requires authentication (a token with
`read:packages`) even for public packages.

Local build: `mvn --file jni/pom.xml verify` (produces the jar plus sources
and javadoc jars in `jni/target/`). Central additionally wants every file PGP
signed and a POM carrying `developers`; both live in the `central` profile
(`mvn deploy -Pcentral`), kept off the default build so neither a signing key
nor a portal token is needed to build or to deploy to GitHub Packages.

## Building

The bindings are part of the main CMake build, toggled by `ODR_JNI` (requires
a JDK, 11+):

```bash
conan install . -o '&:with_jni=True' --build missing
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=conan_toolchain.cmake -DODR_JNI=ON -DODR_TEST=ON
cmake --build build --target odr_jni odr_java odr_java_tests
(cd build/jni && ctest)
```

This produces `build/jni/libodr_jni.dylib` (or `.so`) and
`build/jni/odr-core-java.jar`. `jni/CMakeLists.txt` can also be configured
standalone against an installed `odrcore` package.

## Runtime data

There is none. The renderer's CSS/JS are part of the library and detection needs
no database, so `GlobalParams.setOdrCoreDataPath(...)` and
`setLibmagicDatabasePath(...)` are deprecated leftovers that store a path
nothing reads.

## Notes

- Handle-backed objects (`DecodedFile`, `Document`, `Element`,
  `HtmlService`, ...) own native memory. They free it on garbage collection;
  use `close()` (or try-with-resources) to release large objects eagerly.
- The C++ `HtmlConfig::resource_locator` callback is not exposed; the standard
  resource locator is always used.
- `HttpServer` is available when the native library was built with the HTTP
  server (`Odr.hasHttpServer()`).
