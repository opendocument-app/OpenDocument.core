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
apps do from their WebViews.

Multi-page formats render one view at a time:

```js
for (const view of doc.listViews()) {
  render(doc.render(view.index).html);
}
```

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
- **Enable brotli.** It takes the module from 2.9 M to about 830 K — worth more
  than every code-size flag put together. Hosts that only gzip land at ~1.2 M.
- No COOP/COEP headers needed. The build is deliberately single-threaded so
  that a plain static host, GitHub Pages included, is enough.
- Rendering is synchronous and a large PDF takes seconds, so run the module in
  a Web Worker. Pass `doc.handle` across `postMessage`, never the `Document`.

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
