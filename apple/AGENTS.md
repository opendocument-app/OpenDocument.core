# AGENTS.md — the Apple bindings

Objective-C bindings for the public C++ API (`src/odr/*.hpp`), packaged as
`OdrCoreObjC.framework` and shipped to OpenDocument.ios over Swift Package
Manager. The Apple counterpart of [`../jni`](../jni/AGENTS.md) + [`../android`](../android/AGENTS.md);
read `../jni/AGENTS.md` first — the binding design is the same one, and
`jni/src/*.cpp` is the per-area template.

## Layout

| Path | What |
|------|------|
| `CMakeLists.txt` | The framework target, behind `ODR_APPLE`. |
| `include/OdrCoreObjC/` | The public headers, one per public-API area rather than one per class — ObjC does not need java's one-file-per-type. |
| `src/*.mm` | The bindings. `ODRInternal.{h,mm}` holds string conversion, the error mapping and `guarded`. |
| `module.modulemap` | Explicit, not inferred — an inferred module gives no `export *` control and Swift's importer prefers the real thing. |
| `exported_symbols.txt` | The ld64 export list. |
| `Info.plist.in` | Replaces CMake's template, which carries no platform keys. |
| `build_xcframework.py` | conan + cmake per slice, then `create-xcframework`. `apple/build/` is output, never committed. |

## Slices

An xcframework may not hold two entries for the same *platform*, and device vs
simulator is a platform difference even at the same arch. Which one a binary is
comes from its Mach-O `LC_BUILD_VERSION`, not from the SDK it was built
against, so `build_xcframework.py` asserts it with `vtool` rather than trusting
it — a simulator binary mistagged `IOS` is the classic
"both ios-arm64 represent two equivalent library definitions" rejection.

| slice | profiles |
|-------|----------|
| `ios-arm64` | `apple-ios-armv8` |
| `ios-arm64_x86_64-simulator` | `apple-iossim-armv8` + `apple-iossim-x86_64` |
| `macos-arm64_x86_64` | `apple-macos-armv8` + `apple-macos-x86_64` |

`assemble` also fails the build if a framework is missing its headers, module
map or the plist's platform keys — the analogue of
`android/build.gradle.kts`'s `checkNative`, and for the same reason: those all
publish happily and then fail at the consumer.

## Why a dynamic framework

An undefined symbol becomes a link error here instead of a crash at the
consumer, and a SwiftPM binary target gives the consumer no way to pass
`-ObjC`/`-force_load` for whatever a static archive would drop.

## Rules

- **`ODR` is the public prefix, `OdrCore` the internal one.** The export list
  globs `_OBJC_CLASS_$_ODR*`, so an internal class named `ODR…` would become
  public surface by accident.
- **Export list, never `-fvisibility=hidden`** — the latter hides the ObjC class
  symbols too. Keep ivars out of the headers (properties only), or
  `_OBJC_IVAR_$_ODR*` has to go on the list as well.
- **Do not name anything `version`.** `NSObject` declares `+version` returning
  `NSInteger`; a class property of that name shadows it with an incompatible
  type, and both ObjC and Swift silently resolve to `NSObject`'s. Hence
  `ODROdr.libraryVersion`. Check any new class-level name against `NSObject`.
- **Stage `Headers`/`Resources`/`Modules` explicitly.** CMake's `PUBLIC_HEADER`
  and `RESOURCE` properties copy *nothing* with the Ninja generator on 3.28 —
  the framework builds fine and then cannot be imported. `CMakeLists.txt` does
  it with POST_BUILD commands; do not "simplify" it back.
- **`TARGET_BUNDLE_CONTENT_DIR` is wrong for frameworks** — it expands to the
  bundle root even on macOS, where content lives in `Versions/A`. The
  CMakeLists computes the path itself.
- **Every single call into C++ is guarded — there are no exceptions to this.**
  An exception crossing into ObjC++ unhandled calls `std::terminate`, i.e.
  crashes the host app. Almost nothing in odrcore's public API is `noexcept`,
  so a getter that "obviously cannot fail" still can:
  `odr::Filesystem::exists("")` throws `std::invalid_argument`, which is how
  this rule was learned. Three helpers in `ODRInternal.h`:
  `guarded` where the caller gets an `NSError **`, `guarded_value` for a
  property, and `guarded_void` for a `void` method. Pick a fallback that keeps
  the caller sane — `YES` for a walker's `end`, so a `while (!end)` loop
  terminates instead of spinning. The `NSError` code list mirrors
  `jni/src/odr_jni.cpp::throw_java` — keep the two in step.
- **Elements carry their owner.** Most public C++ handles own a `shared_ptr`,
  so a wrapper holding one by value is self-sufficient and needs no keep-alive.
  `odr::Element` is the exception: it holds a bare pointer into the document's
  adapter. Every `ODRElement` therefore keeps a strong reference to its
  `ODRDocument`, and navigation goes through `-derive:` so that reference is
  carried along by construction rather than by remembering to pass it. This is
  the JNI bindings' owner chain, and it is what lets a caller keep a subtree
  after dropping the document.
- **Strings**: `odr::apple::to_string` / `to_nsstring`, real UTF-8 ↔ UTF-16.
  Never hand a `-UTF8String` pointer to something that outlives the autorelease
  pool.
- **Annotate for Swift** — `NS_SWIFT_NAME`, nullability, lightweight generics,
  `NS_ERROR_ENUM`. The ObjC API is the API; the Swift target on top is only for
  what annotations cannot express, the same way `../android` refuses to
  reimplement the java API in kotlin.
- **Pin `os.version` in every conan profile.** An unset deployment target floats
  with the runner's SDK and would disagree with `Package.swift`'s `platforms:`;
  `CMakeLists.txt` fails the configure rather than let that ship.
- C++ and ObjC++ follow the repo clang-format.

## Releasing

Nothing here cuts a release; the project's flow does (`AGENTS.md`). `release.yml`
calls this workflow with the version it derived, takes the checksum back out and
stamps it into `Package.swift` on the commit it tags.

`Package.swift` must carry the checksum of an artifact built from the commit
that contains `Package.swift`, which is circular because `git_watcher.cmake`
bakes the working-tree sha into every binary. `ODR_GIT_HEAD=v6.2.0` breaks it:
the binary identifies itself by the tag, known before the commit exists, and the
trees then differ only in `Package.swift` — which the framework never sees.

Off a tag the url says `UNRELEASED` and resolves to nothing, so only tags are
consumable. `verify` on `release: published` catches a release cut by hand,
which would leave the tag serving the previous version's binary.

## Testing

The iOS *device* slice is only ever link-checked — nothing runs it. The
simulator suite is the analogue of android's instrumented job and the only
place that sees what a device sees: that the framework loads, that rendering
works with nothing configured, that `temp_directory_path()` is writable inside
an app container. A new binding is only covered once something in `tests/` calls it.

`tests/Fixtures/mixed-layout.odt` is 9 KB of `odt/` from OpenDocument.test,
carried here because `test/data/` is fetched by `cmake/test_data.cmake` and a
package checkout has none of it — and reaching for it as a submodule is the one
thing `Package.swift` must never do. It replaced an ODT the suite built itself,
which proved only that odrcore could read back what the test had written. Text
and CSV need no container and stay inline; keep it that way rather than growing
the fixture set. The same document backs `../jni/testfixtures`, so an assertion
can be compared across the two suites.
