# odr-core-android — the AAR

The JNI bindings (`../jni`) packaged for android: the java API and the native
library for every ABI, in one artifact.

```
odr-core-android.aar
├── classes.jar                        app.opendocument.core (../jni/java)
├── jni/<abi>/libodr_jni.so            the bindings with the core linked in
├── jni/<abi>/libc++_shared.so         the c++ runtime they were built against
└── proguard.txt                       keeps the classes JNI resolves by name
```

ABIs: `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`. minSdk 26.

## Using it

```gradle
repositories {
    mavenCentral()
}

dependencies {
    implementation "app.opendocument:odr-core-android:<version>"
}
```

The same artifact is on GitHub Packages, but prefer Central. GitHub Packages
needs a token with `read:packages` even for a public package. Do not depend on
`odr-core-java` as well, because the AAR carries the same classes.

```kotlin
import app.opendocument.core.*

val file = Odr.open(path)
val service = Html.translate(file, HtmlConfig())
val html = service.bringOffline(outputDir.path)
```

Nothing needs initialising. The renderer's css and js are part of the native
library, and the AAR carries no assets.

Serving the rendered HTML through `HttpServer` needs two things from the app:
`android.permission.INTERNET`, and permission for plain HTTP on loopback,
because android blocks cleartext from API 28 on. A `networkSecurityConfig`
with a `domain-config` for `127.0.0.1` grants only that.

`HttpServer.listen()` blocks, so run it on a thread of the app. `stop()` and
`close()` return only once that thread has left `listen()`, so no join or
timeout is needed. `close()` alone is enough, because it stops the server
before it frees it. A server that outlives one document can stay listening,
with `clear()` between workloads, because rebinding costs a port.

## Building

The AAR needs the native libraries first. `build_native.py` cross compiles
them through conan and cmake into the directory the gradle build reads:

```bash
export ANDROID_HOME=~/Library/Android/sdk
./gradlew assembleRelease                     # builds all four ABIs on the way
./gradlew assembleRelease -Podr.abis=x86_64   # one ABI, for the emulator
./gradlew assembleRelease -Podr.abis=         # none: use what is already there
python build_native.py --abi x86_64           # or run the script directly
```

Gradle properties for what the script cannot guess: `-Podr.conan=<path>` (a
conan outside PATH, for example in a virtualenv), `-Podr.buildProfile=<profile>`
(the conan profile of this machine), `-Podr.python=<path>`.

The odrcore build is a normal one with `ODR_JNI=ON` and the static core linked
into `libodr_jni.so`. The `android-<arch>` conan profiles in
`.github/config/conan/profiles` pin the NDK and API 26.

The libraries ship unstripped, 60 to 72 MB per ABI, so that a consuming app's
`ndk.debugSymbolLevel` can hand play what it needs to symbolicate a crash in
the core. Play serves APKs built from stripped copies, so a device never sees
the weight. This needs both `build_native.py` not stripping and the
`packaging.jniLibs.keepDebugSymbols` rule in `build.gradle.kts`.

## Testing

```bash
./gradlew lint                            # NewApi against minSdk 26, over ../jni/java too
./gradlew connectedDebugAndroidTest       # on a running emulator or device
./gradlew spotlessApply                   # ktfmt, kotlinlang style
```

The instrumented suite (`src/androidTest`) loads the native library, decodes
and renders documents, drives a java log sink from native code, and serves a
document over HTTP. Its inputs come from `../jni/testfixtures`, the same ones
the host junit suite uses.

CI (`.github/workflows/android.yml`) cross compiles each ABI, assembles and
lints the AAR, and runs the suite on API 26 and on a current API level.

## Publishing

Releases go to Maven Central and GitHub Packages, only after the instrumented
suite has passed on every API level.

Maven Central is the one that matters. GitHub Packages demands `read:packages`
even to read a public artifact, which rules out f-droid, because it builds from
source with no credentials. The artifact stays on GitHub Packages because the
maven jar is there too.

Central requires sources and javadoc jars, a POM with `developers`, and a PGP
signature over every file. Signing is conditional on a configured key, so
`publishToMavenLocal` and the GitHub Packages publish work without one. The
portal rejects an unsigned upload, so the release path stays guarded.

Publishing releases the deployment without a click in the portal. The build
waits until the version is published, so a green publish job means it is on
Central, and a rejected bundle fails the release.

OpenDocument.droid still builds odrcore from the conan package
(`with_jni=True`) and deploys `libodr_jni.so` and `odr-core-java.jar` out of
it. The AAR is a second packaging of the same build, for consumers that want a
dependency.
