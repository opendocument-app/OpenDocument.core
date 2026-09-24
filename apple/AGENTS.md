# AGENTS.md — the Apple bindings

Objective-C bindings for the public C++ API (`src/odr/*.hpp`), packaged as
`OdrCoreObjC.framework` and shipped to OpenDocument.ios over Swift Package
Manager. The design is the one in [`../jni/AGENTS.md`](../jni/AGENTS.md).
Read that first. `jni/src/*.cpp` is the per-area template.

## Layout

| Path | What |
|------|------|
| `CMakeLists.txt` | The framework target, behind `ODR_APPLE`. |
| `include/OdrCoreObjC/` | The public headers, one per public-API area. |
| `src/*.mm` | The bindings. `ODRInternal.{h,mm}` holds string conversion, the error mapping and the guards. |
| `swift/` | The Swift layer: optionals, tree walking, `serve`. |
| `module.modulemap` | Explicit, so `export *` is under control. |
| `exported_symbols.txt` | The ld64 export list. |
| `Info.plist.in` | CMake's template carries no platform keys. |
| `build_xcframework.py` | conan plus cmake per slice, then `create-xcframework`. `apple/build/` is output. |
| `tests/` | The XCTest suite. `Fixtures/mixed-layout.odt` is the one fixture. |

## Slices

| slice | profiles |
|-------|----------|
| `ios-arm64` | `apple-ios-armv8` |
| `ios-arm64_x86_64-simulator` | `apple-iossim-armv8`, `apple-iossim-x86_64` |
| `macos-arm64_x86_64` | `apple-macos-armv8`, `apple-macos-x86_64` |

An xcframework refuses two entries for the same platform, and device and
simulator are different platforms. The Mach-O `LC_BUILD_VERSION` says which
one a binary is, not the SDK, so `build_xcframework.py` checks it with
`vtool`. `assemble` also fails if a framework lacks its headers, module map
or the plist's platform keys, because those errors otherwise surface at the
consumer.

The framework is dynamic. An undefined symbol is then a link error here, and
a SwiftPM binary target cannot pass `-ObjC` or `-force_load` for a static
archive.

## Rules

- `ODR` is the public prefix, `OdrCore` the internal one. The export list
  globs `_OBJC_CLASS_$_ODR*`, so an internal `ODR…` class leaks by accident.
- Use the export list, never `-fvisibility=hidden`, which hides the ObjC class
  symbols too. Keep ivars out of the headers, or `_OBJC_IVAR_$_ODR*` has to
  go on the list as well.
- Do not name anything `version`. `NSObject` declares `+version`, and a class
  property of that name resolves to it in both ObjC and Swift. Hence
  `ODROdr.libraryVersion`. Check a new class-level name against `NSObject`.
- Stage `Headers`, `Resources` and `Modules` with the POST_BUILD commands in
  `CMakeLists.txt`. CMake's `PUBLIC_HEADER` and `RESOURCE` properties copy
  nothing with the Ninja generator on 3.28. `TARGET_BUNDLE_CONTENT_DIR` is
  also wrong for a macOS framework, so the CMakeLists computes the path.
- Every call into C++ is guarded, with no exception. An unhandled C++
  exception in ObjC++ calls `std::terminate`. Almost nothing in the public
  API is `noexcept`; `odr::Filesystem::exists("")` throws. `ODRInternal.h` has
  `guarded` for a caller with an `NSError **`, `guarded_value` for a property
  and `guarded_void` for a `void` method. Pick a fallback that keeps the
  caller sane, such as `YES` for a walker's `end`.
- `ODRError` is the head of `odr::ErrorCode` with the same numbers, so
  `error_code()` is a cast and `ODRInternal.mm` static_asserts it. A new code
  needs an enumerator here too. A code past the list reports
  `ODRErrorUnknown`.
- Elements carry their owner. `odr::Element` holds a bare pointer into the
  document's adapter and `odr::HtmlView` one into its service. So every
  `ODRElement` keeps a strong reference to its `ODRDocument`, navigation goes
  through `-derive:` to carry it along, and every `ODRHtmlView` keeps its
  `ODRHtmlService`. Check a new wrapper for a bare pointer before you assume
  the `shared_ptr` makes it self-sufficient.
- Strings go through `odr::apple::to_string` and `to_nsstring`. Never hand a
  `-UTF8String` pointer to something that outlives the autorelease pool.
- Annotate for Swift: `NS_SWIFT_NAME`, nullability, lightweight generics,
  `NS_ERROR_ENUM`. The ObjC API is the API. The Swift target holds only what
  annotations cannot express.
- A boxed `std::optional` is `NS_REFINED_FOR_SWIFT`. The box moves to
  `__name`, and `swift/*+Optionals.swift` carries the real optional. Swift has
  no `@encode`, so `NSValue (ODRTableDimensions)` stays in `ODRTable.h`.
- Pin `os.version` in every conan profile. An unset deployment target floats
  with the runner's SDK and disagrees with `Package.swift`. `CMakeLists.txt`
  fails the configure when it is empty.
- C++ and ObjC++ follow the repo clang-format.

## Releasing

`release.yml` calls `apple.yml` with the derived version, takes the checksum
back and stamps it into `Package.swift` on the commit it tags.
`git_watcher.cmake` bakes the working-tree sha into every binary, which would
make the checksum circular. `ODR_GIT_HEAD=vX.Y.Z` breaks the loop: the binary
identifies itself by the tag, and the trees then differ only in
`Package.swift`, which the framework never sees.

Off a tag the url says `UNRELEASED` and resolves to nothing. `verify` on
`release: published` catches a release cut by hand, which would leave the tag
serving the previous binary.

## Testing

Nothing runs the iOS device slice. It is link-checked only. The simulator
suite is the only place that sees what a device sees: the framework loads,
rendering works with nothing configured, and `temp_directory_path()` is
writable inside an app container. A new binding is covered only once
`tests/` calls it.

`tests/Fixtures/mixed-layout.odt` is 9 KB from OpenDocument.test, carried
here because a package checkout has no `test/data/`, and `Package.swift` must
never reach for a submodule. Text and CSV inputs stay inline. Do not grow the
fixture set. The same document backs `../jni/testfixtures`, so an assertion
can be compared across the two suites.
