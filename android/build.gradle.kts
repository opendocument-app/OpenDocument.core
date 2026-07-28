plugins {
    alias(libs.plugins.android.library)
    alias(libs.plugins.spotless)
    `maven-publish`
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

// An AAR without the native library or without its assets builds and publishes
// happily and then fails at runtime in whatever app picked it up, so make the
// absence a build error.
val checkNative =
    tasks.register("checkNative") {
        dependsOn(buildNative)
        val prebuilt = layout.projectDirectory.dir("native/prebuilt")
        val required = listOf("jniLibs", "assets").map { prebuilt.dir(it).asFile }
        doLast {
            val missing = required.filter { it.list().isNullOrEmpty() }
            if (missing.isNotEmpty()) {
                throw GradleException(
                    "nothing in ${missing.joinToString()} — run android/build_native.py, " +
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

    // the instrumented apk carries the bindings and the 8 MB libmagic database,
    // and pushing that onto a cold emulator outlasts ddmlib's default timeout,
    // which surfaces as a ShellCommandUnresponsiveException rather than as a
    // failing test
    installation { timeOutInMs = 10 * 60 * 1000 }

    sourceSets {
        named("main") {
            // the same java API the maven jar is built from, so the two
            // artifacts cannot drift apart
            java.srcDir("../jni/java")
            jniLibs.srcDir("native/prebuilt/jniLibs")
            assets.srcDir("native/prebuilt/assets")
        }
        named("androidTest") {
            // the inputs the host junit suite builds inline, shared verbatim
            java.srcDir("../jni/testfixtures")
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

    publishing {
        singleVariant("release") {
            withSourcesJar()
            withJavadocJar()
        }
    }
}

dependencies {
    androidTestImplementation(libs.junit)
    androidTestImplementation(libs.androidx.test.junit)
    androidTestImplementation(libs.androidx.test.rules)
    androidTestImplementation(libs.androidx.test.runner)
}

publishing {
    publications {
        register<MavenPublication>("release") {
            groupId = "app.opendocument"
            artifactId = "odr-core-android"
            version = odrVersion

            afterEvaluate { from(components["release"]) }

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
                scm {
                    connection = "scm:git:https://github.com/opendocument-app/OpenDocument.core.git"
                    developerConnection =
                        "scm:git:git@github.com:opendocument-app/OpenDocument.core.git"
                    url = "https://github.com/opendocument-app/OpenDocument.core"
                }
            }
        }
    }

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
