# WebAssembly bindings

`@opendocument/odr-core` renders documents to HTML in the browser, with no
server and no upload. The bytes never leave the machine.

This is not [OpenDocument.js](https://github.com/opendocument-app/OpenDocument.js),
the renderer's own frontend TypeScript, which is compiled into the library and
not published. This package is the library itself.

## Install

```sh
npm install @opendocument/odr-core
```

The package also works straight off a CDN:

```html
<script type="module">
  import { Odr } from 'https://unpkg.com/@opendocument/odr-core';
</script>
```

For a self-hosted copy, every release carries
`odr-core-browser-<version>.zip`: the same files flat, plus an `example.html`
that runs against them. Unzip it where your pages are served from and import
`./index.js`.

## Use

```js
import { Odr } from '@opendocument/odr-core';

const odr = await Odr.load();
// `name` lets a signature-less format like markdown be detected.
const doc = odr.open(new Uint8Array(await file.arrayBuffer()), {
  name: file.name,
});
try {
  const { html } = doc.render(0);
  iframe.src = URL.createObjectURL(new Blob([html], { type: 'text/html' }));
} finally {
  doc.close();
}
```

`html` is a complete document with styles, scripts, images and fonts inline.
A `blob:` iframe keeps the same origin, so the page can reach
`iframe.contentWindow.odr` to drive `search()`, `searchNext()` and the
editing API, as the Android and iOS apps do from their WebViews. The frame
inherits the embedding page's Content-Security-Policy. See
[Content-Security-Policy](#content-security-policy).

Multi-page formats render one view at a time:

```js
for (const view of doc.listViews()) {
  render(doc.render(view.index).html);
}
```

Editing is a round trip through the rendered page:

```js
const doc = odr.open(bytes, { editable: true });
const { html } = doc.render(0);
const page = iframe.contentWindow.odr;
// The mode starts off. `enable()` refuses where the document cannot be edited.
page.editing.enable();
// ... the reader edits the page in the iframe ...
// `getOperations()` returns an object. `edit` wants that object, not a string.
doc.edit(page.editing.getOperations());

const saved = doc.save();   // the document, not the html
download(new Blob([saved]));
// The page and the file agree now, so the log resets and undo starts over.
page.editing.committed();
```

`odr.editing` is on every document view, editable or not. `isEditable()`
tells a host whether to show an edit button, `onEditRefused` says why an edit
was refused, and `onCellsStale` names the formula cells whose input changed.
Nothing recomputes a formula yet. The config keys `keyboardNavigation` and
`keyboardShortcuts` decide whether the page takes the arrow keys and the undo
chord. `editingScope` narrows a document view to edits inside one paragraph,
and the page refuses the rest with code 1010, `outOfScope`.
`example/index.html` wires the whole surface.

`doc.isEditable()` and `doc.isSavable()` answer for this document, where
`doc.capabilities()` answers for the format. ODF, docx, pptx, xlsx and txt
save. Anything else throws `UnsupportedOperation`. A txt saves as UTF-8.

A pdf takes markup annotations through `doc.isAnnotatable()` and
`doc.annotate(annotations)`.

Encrypted documents:

```js
if (doc.isPasswordEncrypted()) {
  try {
    doc.decrypt(password);
  } catch (e) {
    if (e.name === 'WrongPassword') { /* ask again */ }
  }
}
```

Close what you open. A `Document` holds a handle into the wasm heap until you
call `close()`. `using doc = odr.open(...)` works where `Symbol.dispose` is
supported.

## Hosting

- Serve `.wasm` as `application/wasm`, or the browser cannot stream-compile it.
- `script-src 'self' 'wasm-unsafe-eval'` is enough to load the module. It is
  linked with `-sDYNAMIC_EXECUTION=0`, so no `'unsafe-eval'` is needed.
- Enable brotli. It takes the module from about 2.9 MB to about 830 KB. Gzip
  lands at about 1.2 MB.
- No COOP/COEP headers are needed. The build is single-threaded, so a plain
  static host such as GitHub Pages is enough.
- Rendering is synchronous, and a large PDF takes seconds, so run the module
  in a Web Worker. Pass `doc.handle` across `postMessage`, never the
  `Document`.

## Content-Security-Policy

A frame inherits the embedding page's policy, so the embedder's CSP decides
what the document may load, and the failures are quiet.

| Directive | What in the output needs it |
|---|---|
| `font-src data:` | embedded subset fonts (pdf) |
| `img-src data:` | embedded images |
| `style-src 'unsafe-inline'` | the document's `<style>` blocks and its `style` attributes |
| `script-src 'unsafe-inline'` | the renderer's own js, written into every document |

Nothing is fetched from another origin. A policy that works for a document
loaded into a frame:

```
frame-src 'self' blob:; font-src 'self' data:; img-src 'self' data:;
style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'
```

Before you tighten it:

- `font-src data:` fails in a confusing way. A pdf paints text with the code
  points of its embedded subset, so a blocked `@font-face` gives replacement
  boxes, not a fallback face.
- A blocked inline script is silent. The layout is css, so the document still
  looks right. Only search, editing and the sheet and text-view behaviour stop.
  A read-only host can refuse `script-src 'unsafe-inline'`.
- `style-src` cannot be narrowed to a nonce or a hash, because most styling
  is `style` attributes.
- Neither can `script-src`. The renderer's js is embedded in the document, and
  `HtmlConfig::embed_shipped_resources`, which links it as files, is not bound.
- Audio and video stay linked resources, so a media file needs `media-src`.
- `script-src 'unsafe-inline'` also permits `javascript:` urls. The renderer
  refuses every link scheme outside `http`, `https`, `mailto`, `ftp`, `ftps`
  and `tel`, so a document cannot carry one. For untrusted input you can still
  sandbox the frame: `<iframe sandbox="allow-scripts" srcdoc="...">` without
  `allow-same-origin` puts the document in an opaque origin. Search and editing
  keep working inside the frame, but the embedder loses
  `iframe.contentWindow.odr`.

## Building

Needs the Emscripten toolchain, through the conan profile in the repository:

```sh
conan install . --output-folder=build-wasm --build=missing --lockfile-partial \
  --profile:host=emscripten-wasm --profile:build=<your build profile> \
  -o '&:with_wasm=True'
cmake -B build-wasm -DCMAKE_TOOLCHAIN_FILE=build-wasm/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release -DODR_WASM=ON -DODR_CLI=OFF \
  -DODR_WITH_HTTP_SERVER=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build build-wasm --target odr_wasm
```

The package lands in `build-wasm/wasm/dist` and is importable as it is.
`wasm/example/index.html` opens it with no bundler: serve the repository over
HTTP and visit it. Its `edit` and `save` buttons are the reference for wiring
a host to `odr.editing`.

Tests run under node, from ctest with `-DODR_TEST=ON`:

```sh
ctest --test-dir build-wasm/wasm
```
