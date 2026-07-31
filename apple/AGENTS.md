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
| `build_xcframework.py` | conan + cmake per slice, then `create-xcframework`. |

## Why a dynamic framework

`+load` in `src/OdrCoreBootstrap.mm` is what points odrcore at the css/js and
`magic.mgc` in this bundle, so an app never has to. In a **static** framework
nothing references that translation unit, the linker drops it, and the
bootstrap never runs — and a SwiftPM binary target gives the consumer no way to
pass `-ObjC`/`-force_load` to get it back. Two lesser reasons: `.binaryTarget`
has no `resources:`, so the assets have to live in the bundle; and an undefined
symbol becomes a link error here instead of a crash at the consumer.

Consequence to document for consumers: **do not enable mergeable libraries**.
Merging relocates the code into the app binary, `[NSBundle bundleForClass:]`
then returns the app bundle, and the bootstrap points at the wrong place.

## Rules

- **`+load`, not lazy initialisation.** `HtmlConfig::init()` (`src/odr/html.cpp`)
  *snapshots* `GlobalParams::odr_core_data_path()` when constructed, so a hook
  that only fires on the first ObjC call is already too late for a caller that
  reaches odrcore's C++ directly — which OpenDocument.ios does today. It is safe
  this early: Foundation is in the image's `LC_LOAD_DYLIB`, and
  `GlobalParams::instance()` is a function-local static.
- **`ODR` is the public prefix, `OdrCore` the internal one.** The export list
  globs `_OBJC_CLASS_$_ODR*`, so an internal class named `ODR…` would become
  public surface by accident. `OdrCoreBootstrap` is named the way it is for
  exactly that reason.
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

## Testing

The iOS *device* slice is only ever link-checked — nothing runs it. The
simulator suite is the analogue of android's instrumented job and the only
place that sees what a device sees: that `+load` fired, that `NSBundle` found
`magic.mgc`, that `temp_directory_path()` is writable inside an app container.
A new binding is only covered once something in `tests/` calls it.
