# WebAssembly bindings

`@opendocument/odr-core` — render documents to HTML **in the browser**, with no
server and no upload. The bytes never leave the machine.

Not to be confused with [OpenDocument.js](https://github.com/opendocument-app/OpenDocument.js),
which is the renderer's own frontend TypeScript. That is compiled *into* the
library (`src/odr/internal/html/frontend.cpp`) and is not published; this package
is the library itself.

## Install

```sh
npm install @opendocument/odr-core
```

Or skip the build step entirely — the package works straight off a CDN, which
is the least-machinery way to put a viewer on a static host:

```html
<script type="module">
  import { Odr } from 'https://unpkg.com/@opendocument/odr-core';
</script>
```

For a self-hosted copy with no npm and no CDN, every release carries
`odr-core-browser-<version>.zip`: the same files flat, plus an `example.html`
that runs against them. Unzip it where your pages are served from and import
`./index.js`.

## Use

```js
import { Odr } from '@opendocument/odr-core';

const odr = await Odr.load();
const doc = odr.open(new Uint8Array(await file.arrayBuffer()));
try {
  const { html } = doc.render(0);
  iframe.src = URL.createObjectURL(new Blob([html], { type: 'text/html' }));
} finally {
  doc.close();
}
```

`html` is a complete document — styles, scripts, images and fonts all inline —
so it needs nothing fetched alongside it. A `blob:` iframe keeps the same
origin, so the page can still reach `iframe.contentWindow.odr` to drive
`search()`, `searchNext()` and `generateDiff()`, exactly as the Android and iOS
apps do from their WebViews. The frame inherits the embedding page's
Content-Security-Policy, so a page that ships one has to allow what the
document carries — see [Content-Security-Policy](#content-security-policy).

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
// ... the reader edits the page in the iframe ...
// `generateDiff()` returns the json already; stringifying it again is a string
// where `edit` wants an object, and it throws.
doc.edit(iframe.contentWindow.odr.generateDiff());

const saved = doc.save();   // the document, not the html
download(new Blob([saved]));
```

`isEditable()` and `isSavable()` answer for this document, where
`capabilities()` answers for the format. Only ODF and docx can be saved so far;
anything else throws `UnsupportedOperation`.

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

**Close what you open.** JS has no destructors, so a `Document` holds a handle
into the wasm heap until you say otherwise. `using doc = odr.open(...)` works
where `Symbol.dispose` is supported.

## Hosting

- Serve `.wasm` as `application/wasm`, or the browser cannot stream-compile it.
- `script-src 'self' 'wasm-unsafe-eval'` is enough to load the module. It is
  linked with `-sDYNAMIC_EXECUTION=0`, so embind builds its invokers without
  `new Function` and no `'unsafe-eval'` is needed.
- **Enable brotli.** It takes the module from 2.9 M to about 830 K — worth more
  than every code-size flag put together. Hosts that only gzip land at ~1.2 M.
- No COOP/COEP headers needed. The build is deliberately single-threaded so
  that a plain static host, GitHub Pages included, is enough.
- Rendering is synchronous and a large PDF takes seconds, so run the module in
  a Web Worker. Pass `doc.handle` across `postMessage`, never the `Document`.

## Content-Security-Policy

The rendered html is self-contained, but a frame inherits the embedding page's
policy — so the *embedder's* CSP decides what the document may load, and the
failures are quiet. Measured across an odt, ods, docx and pdf:

| Directive | What in the output needs it | Seen in |
|---|---|---|
| `font-src data:` | embedded subset fonts | pdf (7 of 8 `@font-face`) |
| `img-src data:` | embedded images | odt, docx, standalone images |
| `style-src 'unsafe-inline'` | the document's own `<style>` blocks **and** its `style` attributes | every format (2–3 blocks, and up to hundreds of attributes) |
| `script-src 'unsafe-inline'` | the renderer's own js, written into every document | every format but a standalone image (1–3) |

Nothing is fetched from another origin, so no host has to be allow-listed. A
policy that works, for a document loaded into a frame:

```
frame-src 'self' blob:; font-src 'self' data:; img-src 'self' data:;
style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'
```

### `script-src 'unsafe-inline'` is not free

It also permits **`javascript:` urls**, and a document may carry one: odrcore
filters a pdf link action down to an allowlist of navigable schemes, but a
hyperlink in an odt or a docx is only attribute-escaped. In a `blob:` frame —
same origin as the embedder, which is what lets the page call
`iframe.contentWindow.odr` — such a link runs with the embedder's origin.

Fine for documents you trust. For untrusted input, sandbox the frame and give
up the same-origin API:

```html
<iframe sandbox="allow-scripts" srcdoc="..."></iframe>
```

`allow-scripts` **without** `allow-same-origin` puts the document in an opaque
origin: search and editing still work inside the frame, while a `javascript:`
link can no longer reach the embedding page. Granting both together is the same
as not sandboxing at all.

Narrowing to a hash or a nonce is not possible from here: the renderer's js is
embedded in the document, and `HtmlConfig::embed_shipped_resources` — which
links it as files instead — is not bound in this build.

Worth knowing before tightening the rest:

- **`font-src data:` is the one that fails most confusingly.** A pdf's text is
  painted with the code points its embedded subset defines, so a blocked
  `@font-face` does not fall back to a system face — every glyph comes out as a
  replacement box.
- **A blocked inline script is silent.** The layout is css, so the document
  still looks right; only search, editing and the spreadsheet and text-view
  behaviour stop working. Refusing `script-src 'unsafe-inline'` is a real
  option when the document only has to be *read*.
- **`style-src` cannot be narrowed to a nonce or a hash.** Most of the styling
  is `style` attributes, which only `'unsafe-inline'` (or `'unsafe-hashes'`)
  covers.
- **Audio and video stay linked resources** rather than data urls, so a media
  file needs `media-src` pointing wherever the resource was written.

Loading the module itself is a separate question — see [Hosting](#hosting).

## Building

Needs the Emscripten toolchain, via the conan profile in the repository:

```sh
conan install . --output-folder=build-wasm --build=missing --lockfile-partial \
  --profile:host=emscripten-wasm --profile:build=<your build profile> \
  -o '&:with_wasm=True'
cmake -B build-wasm -DCMAKE_TOOLCHAIN_FILE=build-wasm/conan_toolchain.cmake \
  -DODR_WASM=ON -DODR_CLI=OFF -DODR_WITH_HTTP_SERVER=OFF -DBUILD_SHARED_LIBS=OFF
cmake --build build-wasm --target odr_wasm
```

The package lands in `build-wasm/wasm/dist` and is directly importable.
`wasm/example/index.html` opens it with no bundler; serve the repository over
HTTP and visit it.

Tests run under node, from ctest with `-DODR_TEST=ON`:

```sh
ctest --test-dir build-wasm/wasm
```
