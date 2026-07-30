# odr-core-android — the AAR

The JNI bindings (`../jni`) packaged for android: the java API, the native
library for every ABI, and the runtime data the renderer needs, in one artifact.

```
odr-core-android.aar
├── classes.jar                        app.opendocument.core (../jni/java) + OdrAndroid
├── jni/<abi>/libodr_jni.so            the bindings with the core linked in
├── jni/<abi>/libc++_shared.so         the c++ runtime they were built against
├── assets/core/odrcore/*              css/js of the html renderer
├── assets/core/libmagic/magic.mgc     libmagic database
└── proguard.txt                       keeps the classes JNI resolves by name
```

ABIs: `arm64-v8a`, `armeabi-v7a`, `x86`, `x86_64`. minSdk 26.

## Using it

```gradle
repositories {
    maven {
        url = uri("https://maven.pkg.github.com/opendocument-app/OpenDocument.core")
        credentials {
            username = providers.gradleProperty("gpr.user").orNull ?: System.getenv("GITHUB_ACTOR")
            password = providers.gradleProperty("gpr.key").orNull ?: System.getenv("GITHUB_TOKEN")
        }
    }
}

dependencies {
    implementation "app.opendocument:odr-core-android:<version>"
}
```

GitHub Packages needs a token with `read:packages` even for public packages —
see [Publishing](#publishing) for what that rules out. Do not depend on
`odr-core-java` as well: the AAR carries the same classes.

```kotlin
import app.opendocument.core.*
import app.opendocument.core.android.OdrAndroid

OdrAndroid.init(context) // extracts the bundled assets, loads the library

val file = Odr.open(path)
val service = Html.translate(file, cacheDir.path, HtmlConfig())
val html = service.bringOffline(outputDir.path)
```

The java API is java and stays callable as such (`OdrAndroid.init(context)` is
a static method, and it still throws a checked `IOException`); only this
module's own code — `OdrAndroid` and the instrumented suite — is kotlin.

`OdrAndroid.init` is idempotent and cheap after the first call: the assets are
unpacked once per library build, into the app's no-backup storage, and the
library is pointed at them via `GlobalParams`.

Serving the rendered HTML through `HttpServer` needs two things from the app,
neither of which a library may decide on its own: `android.permission.INTERNET`,
and permission for plain HTTP on loopback (android blocks cleartext from API 28
on) — a `networkSecurityConfig` with a `domain-config` for `127.0.0.1` is the
narrow way to grant it.

`HttpServer.listen()` blocks, so it runs on a thread of the app's. `stop()` and
`close()` return only once that thread is back out of it, so tearing the server
down needs no join or timeout of its own — and `close()` on its own is enough,
it stops the server before freeing it. A server that outlives one document is
best left listening with `clear()` between workloads: rebinding costs a port.

## Building

The AAR needs the native libraries first; `build_native.py` cross compiles them
through conan and cmake and lays them out where the gradle build reads them:

```bash
export ANDROID_HOME=~/Library/Android/sdk
./gradlew assembleRelease                     # builds all four ABIs on the way
./gradlew assembleRelease -Podr.abis=x86_64   # just one, for the emulator
./gradlew assembleRelease -Podr.abis=         # none: use what is already there
python build_native.py --abi x86_64           # or drive it directly
```

Anything the script needs but cannot guess is a gradle property:
`-Podr.conan=<path>` (a conan outside PATH, e.g. in a virtualenv),
`-Podr.buildProfile=<profile>` (the conan profile of *this* machine),
`-Podr.python=<path>`.

The odrcore build is a normal one — `ODR_JNI=ON`, static core linked into
`libodr_jni.so` — driven by the `android-<arch>` conan profiles in
`.github/config/conan/profiles`, which pin the NDK and API 26.

## Testing

```bash
./gradlew lint                            # NewApi against minSdk 26, over ../jni/java too
./gradlew connectedDebugAndroidTest       # on a running emulator or device
./gradlew spotlessApply                   # ktfmt, kotlinlang style
```

The instrumented suite (`src/androidTest`) is the part that sees what a device
sees: it loads the native library, extracts and reads the bundled assets,
decodes and renders documents, drives a java log sink from native code, and
serves a document over HTTP. Its inputs come from `../jni/testfixtures`, the
same builder the host junit suite uses.

CI (`.github/workflows/android.yml`) cross compiles each ABI, assembles and
lints the AAR, and runs the suite on API 26 — the floor OpenDocument.droid
ships to — and on a current API level.

## Publishing

Releases go to GitHub Packages, like the maven jar, and only once the
instrumented suite has passed on every API level — an AAR that assembles and
lints is no evidence that it works on a device. That is enough for
consumers who can hold a token, and **not** enough for the one this is
ultimately built for: GitHub Packages demands `read:packages` even to read a
public artifact, and f-droid builds from source with no credentials at all.
Making the AAR the base of OpenDocument.droid therefore means publishing it to
Maven Central first, which is a separate piece of work — the `app.opendocument`
namespace has to be verified, and every artifact signed.

Until then OpenDocument.droid keeps building odrcore from the conan package
(`with_jni=True`), deploying `libodr_jni.so`, `odr-core-java.jar` and the assets
out of it. That path is unaffected by anything here, and it is the reason the
two halves cannot drift: they come out of one build.

So the AAR is, for now, a second packaging of that same build — for consumers
that just want a dependency, and the reason android gets compiled and exercised
on every push at all.
