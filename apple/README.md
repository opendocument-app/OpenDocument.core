# OdrCore — odrcore for iOS and macOS

Objective-C bindings for [OpenDocument.core](../README.md), shipped as a binary
`OdrCoreObjC.xcframework` with a thin Swift layer on top. Decode office
documents and render them to HTML.

## Install

```swift
.package(url: "https://github.com/opendocument-app/OpenDocument.core", from: "7.3.0")
```

Depend on the `OdrCore` product and `import OdrCore`. Requires iOS 15 or
macOS 12.

## Render a document

```swift
let file = try DecodedFile.decode(path: path)
let config = HtmlConfig()
let service = try HtmlTranslator.translate(file: file, config: config)

for view in service.views {
  var resources: NSArray?
  let html = try view.writeHtml(resources: &resources)
}
```

The renderer's css and JS are part of the library and go into the HTML it
writes. Optional settings of `HtmlConfig` are Swift optionals:

```swift
config.spreadsheetLimit = TableDimensions(rows: 100_000, columns: 500)
config.initialZoom = 1.5
config.pageRangeEnd = nil  // to the last page
```

## Serve it into a web view

OpenDocument.ios renders on demand and serves over loopback:

```swift
let service = try HtmlTranslator.translate(file: file, config: HtmlConfig())

let server = HttpServer()
try server.connect(service, prefix: "doc")
let handle = try server.serve()          // binds 127.0.0.1, listens off-thread

let view = service.views[0]
webView.load(URLRequest(url: handle.url(prefix: "doc")
  .appendingPathComponent(view.path)))
```

`handle.stop()` blocks until the server has stopped. Releasing the handle
does the same. Both are idempotent. Call `stop()` off the main thread: once
anything has been served it takes about five seconds, because the accept loop
waits out the web view's keep-alive connection
([#641](https://github.com/opendocument-app/OpenDocument.core/issues/641)).

Bind `127.0.0.1`, the default of `serve()`. `0.0.0.0` triggers the iOS Local
Network permission prompt, and nothing off the device needs the server.

## Walk the document

```swift
let root = try document.rootElement()
for text in root.descendants(ofType: Text.self) {
  print(text.content, text.style.fontSize?.stringValue ?? "")
}
```

Navigation returns the most derived type a node qualifies for, so `as?
Paragraph` is enough. An element keeps its document alive, so a subtree stays
valid after you drop the `Document`.

## Errors

Everything that can fail is `throws`, under `ODRErrorDomain`:

```swift
do {
  _ = try DecodedFile.decode(path: path)
} catch let error as NSError where error.code == ODRError.wrongPassword.rawValue {
  // prompt for the password
}
```

## Building it yourself

```bash
apple/build_xcframework.py slice        # every slice, conan + cmake
apple/build_xcframework.py assemble     # lipo + create-xcframework
ODR_XCFRAMEWORK=OdrCoreObjC.xcframework swift test
```

`ODR_XCFRAMEWORK` is relative to the package root, because SwiftPM rejects an
absolute path for a binary target. See [`AGENTS.md`](AGENTS.md) for how the
pieces fit.
