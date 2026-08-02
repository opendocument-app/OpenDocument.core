import com.vanniktech.maven.publish.AndroidSingleVariantLibrary
import com.vanniktech.maven.publish.DeploymentValidation

plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.spotless)
    alias(libs.plugins.maven.publish)
}

// ktfmt in the kotlinlang flavor, the same formatter and version
// OpenDocument.droid runs, so a file can move between the two repos unchanged.
// ./gradlew spotlessApply to fix, spotlessCheck to verify (the format workflow
// does). Only this module's own kotlin: ../jni/java is java, formatted by hand
// to the google-java-format style, and reformatting it from here would rewrite
// the maven jar's sources as a side effect of an android build.
spotless {
    kotlin {
        target("src/*/java/**/*.kt")

        ktfmt(libs.versions.ktfmt.get()).kotlinlangStyle()
        trimTrailingWhitespace()
        endWithNewline()
    }
}

// The version is injected on release, the way the maven jar takes -Drevision.
// Everything else is a snapshot, which nothing consumes by version.
val odrVersion =
    providers.gradleProperty("odr.version")
        .orElse(providers.environmentVariable("ODR_VERSION"))
        .filter { it.isNotEmpty() }
        .map { it.removePrefix("v") }
        .getOrElse("0.0.0-SNAPSHOT")

// Which ABIs `build_native.py` is asked for. Set it to nothing
// (`-Podr.abis=`) when the libraries were built elsewhere and dropped into
// native/prebuilt, which is what CI does with its per-ABI build artifacts.
val nativeAbis =
    providers.gradleProperty("odr.abis")
        .getOrElse("armv7,armv8,x86,x86_64")
        .split(",")
        .map { it.trim() }
        .filter { it.isNotEmpty() }

val buildNative =
    tasks.register<Exec>("buildNative") {
        description = "Builds libodr_jni.so for odr.abis into native/prebuilt"
        commandLine(
            buildList {
                add(providers.gradleProperty("odr.python").getOrElse("python3"))
                add(file("build_native.py").absolutePath)
                nativeAbis.forEach {
                    add("--abi")
                    add(it)
                }
                providers.gradleProperty("odr.conan").orNull?.let {
                    add("--conan")
                    add(it)
                }
                providers.gradleProperty("odr.buildProfile").orNull?.let {
                    add("--build-profile")
                    add(it)
                }
            }
        )
        // conan and cmake do their own up-to-date checking, and modelling the
        // whole C++ tree as inputs here would only duplicate it badly — a task
        // that declares no outputs runs every time, which is what we want
        enabled = nativeAbis.isNotEmpty()
    }

// An AAR without the native library builds and publishes happily and then
// fails at runtime in whatever app picked it up, so make the absence a build
// error.
val checkNative =
    tasks.register("checkNative") {
        dependsOn(buildNative)
        val jniLibs = layout.projectDirectory.dir("native/prebuilt/jniLibs").asFile
        doLast {
            if (jniLibs.list().isNullOrEmpty()) {
                throw GradleException(
                    "nothing in $jniLibs — run android/build_native.py, " +
                        "or pass -Podr.abis=<abis> to have the build run it"
                )
            }
        }
    }

tasks.named("preBuild") { dependsOn(checkNative) }

android {
    namespace = "app.opendocument.core"
    compileSdk = 36

    defaultConfig {
        minSdk = 26
        testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"
        consumerProguardFiles("consumer-rules.pro")
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_17
        targetCompatibility = JavaVersion.VERSION_17
    }

    packaging {
        jniLibs {
            // AGP strips the libraries on their way into the AAR unless told not
            // to, silently — the published AAR comes out byte-identical to a
            // fully stripped one however `build_native.py` built it.
            keepDebugSymbols += "**/*.so"
        }
    }

    // pushing the instrumented apk onto a cold emulator outlasts ddmlib's
    // default timeout, which surfaces as a ShellCommandUnresponsiveException
    // rather than as a failing test
    installation { timeOutInMs = 10 * 60 * 1000 }

    sourceSets {
        named("main") {
            // the same java API the maven jar is built from, so the two
            // artifacts cannot drift apart
            java.srcDir("../jni/java")
            jniLibs.srcDir("native/prebuilt/jniLibs")
        }
        named("androidTest") {
            // the host junit suite's inputs, shared verbatim
            java.srcDir("../jni/testfixtures")
            // TestFiles reads the odt off the classpath, so it has to be
            // packaged into the test apk as a java resource — not an asset,
            // which only the android half of the suite could reach
            resources.srcDir("../jni/testfixtures/resources")
        }
    }

    lint {
        // The static half of the android guard, over the java API of the maven
        // jar too, since that is the same source set. NewApi covers android.*
        // and the java.* APIs that core library desugaring cannot backport —
        // `java.lang.ref.Cleaner` of #621 among them — but it stays quiet about
        // the desugarable ones (`List.of`, `Optional.isEmpty`), so the
        // instrumented run on an API 26 emulator is what actually settles it.
        abortOnError = true
        warningsAsErrors = true
        checkTestSources = true
        // consumed by github code scanning, so findings show up inline on PRs
        sarifReport = true

        // "a newer compileSdk is available" is a calendar event, not a defect,
        // and as an error it breaks CI the day google ships an SDK
        disable += "GradleDependency"
        // the java API is shared with the desktop jar, where
        // `System.load(<absolute path>)` is how a caller points the JVM at a
        // library outside java.library.path; android never takes that branch
        disable += "UnsafeDynamicallyLoadedCode"
    }
}

dependencies {
    androidTestImplementation(libs.junit)
    androidTestImplementation(libs.androidx.test.junit)
    androidTestImplementation(libs.androidx.test.rules)
    androidTestImplementation(libs.androidx.test.runner)
}

// Two destinations, deliberately. Maven Central is the one that matters: it is
// the only one f-droid and any other credential-less source builder can resolve
// from, and `app.opendocument` is already a live namespace there
// (`pdf2htmlex-android`, `wvware-android`). GitHub Packages stays because the
// maven jar publishes there and dropping it would break whoever already reads
// it. Central mandates what the publication below carries — sources and javadoc
// jars, a pom with developers, and a PGP signature over every file — none of
// which GitHub Packages asks for.
mavenPublishing {
    configure(AndroidSingleVariantLibrary(variant = "release"))

    // Uploads to the portal and stops. A human releases it from there, so a bad
    // artifact is still recallable — Central is immutable once released.
    //
    // Waiting for VALIDATED is what makes a failed deployment fail the release:
    // the default uploads, prints "Skipping deployment validation!" and exits 0
    // whatever the portal then makes of the bundle.
    publishToMavenCentral(false, DeploymentValidation.VALIDATED)

    // Only Central demands a signature. Making it unconditional would mean no
    // `publishToMavenLocal` and no GitHub Packages publish without a private
    // key on hand, which is a steep price for a repository that never checks
    // one. A Central upload missing its .asc files is rejected by the portal,
    // so the release path is still guarded.
    if (providers.gradleProperty("signingInMemoryKey").isPresent) {
        signAllPublications()
    }

    coordinates("app.opendocument", "odr-core-android", odrVersion)

    pom {
        name = "odr-core-android"
        description =
            "Android bindings for OpenDocument.core — decode office documents " +
                "(ODF, OOXML, legacy MS binary, PDF, CSV, ...) and render them to HTML"
        url = "https://github.com/opendocument-app/OpenDocument.core"
        licenses {
            license {
                name = "MPL-2.0"
                url = "https://www.mozilla.org/en-US/MPL/2.0/"
            }
        }
        developers {
            developer {
                id = "opendocument-app"
                name = "OpenDocument.app"
                url = "https://github.com/opendocument-app"
            }
        }
        scm {
            connection = "scm:git:https://github.com/opendocument-app/OpenDocument.core.git"
            developerConnection = "scm:git:git@github.com:opendocument-app/OpenDocument.core.git"
            url = "https://github.com/opendocument-app/OpenDocument.core"
        }
    }
}

publishing {
    repositories {
        maven {
            name = "GitHubPackages"
            url = uri("https://maven.pkg.github.com/opendocument-app/OpenDocument.core")
            credentials {
                username =
                    providers.gradleProperty("gpr.user")
                        .orElse(providers.environmentVariable("GITHUB_ACTOR"))
                        .orNull
                password =
                    providers.gradleProperty("gpr.key")
                        .orElse(providers.environmentVariable("GITHUB_TOKEN"))
                        .orNull
            }
        }
    }
}
