// swift-tools-version: 5.9

import Foundation
import PackageDescription

// The bindings ship as a prebuilt xcframework — odrcore is a large C++ project
// with conan dependencies, which SwiftPM cannot build.
//
// The url and checksum below are stamped by `release.yml` onto the commit it
// tags, so off a release tag they say `UNRELEASED` and resolve to nothing. Only
// tags are consumable.
//
// `ODR_XCFRAMEWORK` points at a locally built one, which is how the test target
// and a developer working on `apple/` exercise what they just built rather than
// the last release. Consumers never set it and get the release artifact.
//
// It must be **relative to the package root** — SwiftPM rejects an absolute
// path for a binary target outright. `apple/build_xcframework.py assemble`
// writes to the repo root, so the value is normally just the bundle name:
//
//     ODR_XCFRAMEWORK=OdrCoreObjC.xcframework swift build
let binary: Target = ProcessInfo.processInfo.environment["ODR_XCFRAMEWORK"]
    .map { Target.binaryTarget(name: "OdrCoreObjC", path: $0) }
    ?? Target.binaryTarget(
        name: "OdrCoreObjC",
        url:
            "https://github.com/opendocument-app/OpenDocument.core/releases/download/v6.2.0/OdrCoreObjC.xcframework.zip",
        checksum: "4d815ec9e827c548ab674d5625cdd8f9688f16b128c5166fb8d89993e067de4d"
    )

let package = Package(
    name: "OdrCore",
    // Must not be below what the slices were built for; the conan `apple-*`
    // profiles pin the same values.
    platforms: [
        .iOS(.v15),
        .macOS(.v12),
    ],
    products: [
        .library(name: "OdrCore", targets: ["OdrCore"])
    ],
    targets: [
        binary,
        // Everything Apple lives under `apple/`, so the targets point there
        // rather than at the conventional `Sources/`.
        .target(name: "OdrCore", dependencies: ["OdrCoreObjC"], path: "apple/swift"),
        // `.copy`, not `.process`: the fixture is an input to decode verbatim,
        // and processing an .odt would be a resource pipeline guessing at a zip.
        .testTarget(
            name: "OdrCoreTests", dependencies: ["OdrCore"], path: "apple/tests",
            resources: [.copy("Fixtures")]),
    ]
)
