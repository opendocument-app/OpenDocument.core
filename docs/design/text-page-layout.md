# Text page-layout markup

Core exposes its existing page/master model to browser renderers without
deciding where automatic page boundaries fall. The visible fallback stays in
`.odr-pagination-original`; inert templates follow it.

- `template[data-odr-page-template]` describes the root layout and each reachable
  master page. Its key is `root`, a master ID, or `<id>-first` for first-page
  marginals. `data-odr-next-layout` follows the master graph; cycles are visited
  once. `data-odr-master-page` identifies the underlying master.
- Page width, height, margins and marginal spacing use the model's CSS lengths.
  A master missing width or height inherits that dimension from the root.
- Headers and footers reference `template[data-odr-marginal-template]` through
  `data-odr-marginal`. Each marginal is rendered once, without editing addresses.
  Its images are self-contained rather than referring to shared SVG definitions
  inside another template.
- Paragraphs with an authored master transition carry `data-odr-master-page`.
  Explicit break elements carry `data-odr-page-break`; paragraph breaks retain
  their `break-before`/`break-after` CSS. Legacy page-box boundaries can also
  represent breaks consumed by the fallback renderer.

This adds no format-parser support. Core knows authored layouts and transitions;
the browser's font metrics determine automatic breaks. The templates do not
themselves activate pagination or change the document's editing protocol.
