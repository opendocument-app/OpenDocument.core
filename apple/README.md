# OdrCore — odrcore for iOS and macOS

Objective-C bindings for [OpenDocument.core](../README.md), shipped as a binary
`OdrCoreObjC.xcframework` with a thin Swift layer on top. Decode office
documents (ODF, OOXML, legacy MS binary, PDF, CSV, …) and render them to HTML.

## Install

```swift
.package(url: "https://github.com/opendocument-app/OpenDocument.core", from: "6.2.0")
```

then depend on the `OdrCore` product. One import gets you everything:

```swift
import OdrCore
```

Requires iOS 15 or macOS 12.

## Render a document

```swift
let file = try DecodedFile.decode(path: path)
let config = HtmlConfig()
let service = try HtmlTranslator.translate(
  file: file, cachePath: cacheDirectory, config: config)

for view in service.views {
  var resources: NSArray?
  let html = try view.writeHtml(resources: &resources)
}
```

Nothing needs configuring first. The framework points odrcore at the css, JS and
libmagic database it carries before `main` runs — that is what the `+load` in
`OdrCoreBootstrap.mm` is for. Override it with `GlobalParams` from
`application(_:didFinishLaunchingWithOptions:)` if you relocated the resources.

## Serve it into a web view

Rendering on demand and serving over loopback is what OpenDocument.ios does, and
it beats writing every page to disk up front.

```swift
let config = HtmlConfig()
config.relativeResourcePaths = false   // odrcore rejects these in server mode

let service = try HtmlTranslator.translate(
  file: file, cachePath: cacheDirectory, config: config)

let server = HttpServer()
try server.connect(service, prefix: "doc")
let handle = try server.serve()          // binds 127.0.0.1, listens off-thread

let view = service.views[0]
webView.load(URLRequest(url: handle.url(prefix: "doc")
  .appendingPathComponent(view.path)))
```

`handle.stop()` stops the server and blocks until it has stopped; releasing the
handle does the same. Both are idempotent.

Keep that off the main thread for now: stopping takes about five seconds once
anything has been served, because the accept loop waits out the web view's
keep-alive connection
([#641](https://github.com/opendocument-app/OpenDocument.core/issues/641)).

Bind `127.0.0.1`, which is what `serve()` defaults to. `0.0.0.0` triggers the
iOS Local Network permission prompt, and nothing off the device needs to reach
a server that exists to feed a web view. A thread blocked in `listen()` is also
subject to the app being suspended in the background.

## Walk the document

```swift
let root = try document.rootElement()
for text in root.descendants(ofType: Text.self) {
  print(text.content, text.style.fontSize?.stringValue ?? "")
}
```

Navigation returns the most derived type a node qualifies for, so `as? Paragraph`
is enough. Elements keep their document alive, so a subtree stays valid after you
drop the `Document`.

## Errors

Everything that can fail is `throws`, under `ODRErrorDomain`:

```swift
do {
  _ = try DecodedFile.decode(path: path)
} catch let error as NSError where error.code == ODRError.wrongPassword.rawValue {
  // prompt for the password
}
```

## Do not enable mergeable libraries

Merging relocates the framework's code into your app binary, at which point
`Bundle(for:)` returns the app bundle and the bundled resources are no longer
found. Embed and sign it as a normal dynamic framework, which is what SwiftPM
does by default.

## Building it yourself

```bash
apple/build_xcframework.py slice        # every slice, conan + cmake
apple/build_xcframework.py assemble     # lipo + create-xcframework
ODR_XCFRAMEWORK=OdrCoreObjC.xcframework swift test
```

`ODR_XCFRAMEWORK` is relative to the package root — SwiftPM rejects an absolute
path for a binary target. See [`AGENTS.md`](AGENTS.md) for how the pieces fit.
