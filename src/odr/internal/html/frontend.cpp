#include <odr/internal/html/frontend.hpp>

#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/xml/xml_util.hpp>

#include <algorithm>
#include <array>
#include <span>
#include <string>
#include <string_view>

namespace odr::internal::html {

namespace {

/// Every page we write states its own body margin and background, rather than
/// reading whatever the browser's default sheet says.
constexpr std::string_view document_css = R"css(
body{margin:0;background:#fff}
/* What the formats anchor against: a page for shapes, a paragraph or a cell
   for frames. */
x-p,td,.odr-page-outer{position:relative}
/* Word and odf start a cell at its top where the browser default centres it; a
   sheet says its own, more specifically. */
td{vertical-align:top}
x-p{display:block}
x-s{display:inline}
.odr-background{padding:0;background:#525659}
/* The page column, sized to the widest page so pages of differing width centre
   against each other, not against the viewport. The page's side margin is part
   of that width, so fitting the document to a phone screen leaves a gutter
   instead of going edge to edge. */
.odr-pages{display:flex;flex-direction:column;align-items:center;gap:16px;padding:max(16px,var(--odr-min-margin-top,0px)) 0 max(16px,var(--odr-min-margin-bottom,0px));width:max-content;min-width:100%}
/* A stacking context, for the shape backgrounds a page holds at `z-index:-1`.
   Not a negative `z-index`, which is one too but takes the page out of reach of
   hit testing. */
.odr-page-outer{display:flex;margin:0 max(16px,var(--odr-min-margin-right,0px)) 0 max(16px,var(--odr-min-margin-left,0px));background:#fff;box-shadow:0 1px 4px rgba(0,0,0,.5);isolation:isolate}
/* Reflowed to the viewport there is no page box to inset the text. A physical
   measure, like the page margin it stands in for; `--odr-min-margin-*` is the
   floor the config puts under it, and is unset by default. */
.odr-text-flow{padding:max(3mm,var(--odr-min-margin-top,0px)) max(3mm,var(--odr-min-margin-right,0px)) max(3mm,var(--odr-min-margin-bottom,0px)) max(3mm,var(--odr-min-margin-left,0px))}
/* The label is text rather than a `::marker`, which no selection would copy.
   It hangs into the item's padding so wrapped lines align under the text. */
.odr-list-item{padding-left:2em}
.odr-list-marker{display:inline-block;min-width:2em;margin-left:-2em;white-space:pre}
)css";

/// Dark counterpart of the style above. The `media` attribute of the element
/// carrying it gates it — a rule inside a query cannot beat an inline color.
constexpr std::string_view document_dark_css = R"css(
:root{color-scheme:dark}
body{background:#0d1117;color:#e6edf3}
/* The step to the page is what makes it read as a sheet. */
.odr-background{background:#010409}
.odr-page-outer{background-color:#161b22!important;box-shadow:0 1px 4px rgba(0,0,0,.8)}
/* Fills give way to the page, text to one legible against it.
   `background-image` is left alone: a page printed on a picture keeps it. */
div,table,tr,td,x-p,x-s{background-color:transparent!important}
/* A shape's fill is an svg `fill` (`translate_drawing_style`), not a
   background, and gives way like one. The stroke stays: it is the line. */
svg,svg *{fill:transparent!important}
td,x-p,x-s{color:#e6edf3!important}
a,a x-p,a x-s{color:#6cb6ff!important}
)css";

/// No `text-overflow`: it sat on `td`, whose overflow is visible, and making it
/// work would mean clipping.
constexpr std::string_view spreadsheet_css = R"css(
:root{
--odr-sheet-line:#e3e5e8;
--odr-sheet-rule:#c8ccd1;
--odr-sheet-ruler:#f5f6f7;
--odr-sheet-ruler-text:#5c6169;
--odr-sheet-canvas:#eceef1;
--odr-sheet-wash:rgba(0,0,0,.045);
--odr-sheet-wash-pinned:rgba(0,0,0,.09);
--odr-sheet-wash-ruler:rgba(0,0,0,.10);
--odr-sheet-focus:#3c78dc;
--odr-sheet-refused:#d1493f;
--odr-sheet-raised:#ffffff;
--odr-sheet-font:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;
}
/* A sheet is not a page: past the last row and column is canvas. */
body{margin:0;background:var(--odr-sheet-canvas)}
.odr-sheet{background:#fff;border-collapse:collapse;table-layout:fixed}
/* The sheet's own cells, not a table the document itself drew inside one. The
   font is what anything in a cell falls back to, a string written straight
   into it included. */
.odr-sheet>tbody>tr>td{vertical-align:bottom;height:inherit;padding:1px 6px;font-family:var(--odr-sheet-font);font-size:10pt}
/* Exactly the cell's height, which is what holds a row to the height the file
   states. `translate_sheet` says per cell what a line too long for it does. */
.odr-sheet>tbody>tr>td>x-p{height:inherit}
/* Sticky cells in a collapsed border model do not repaint their borders in
   Chrome or WebKit, so the ruler uses inset shadows. */
.odr-sheet th{position:sticky;background:var(--odr-sheet-ruler);color:var(--odr-sheet-ruler-text);font:500 12px/1.6 var(--odr-sheet-font);text-align:center;vertical-align:middle;padding:0 4px;white-space:nowrap;user-select:none}
.odr-sheet thead th{top:0;z-index:2;height:24px;box-shadow:inset 0 -1px 0 var(--odr-sheet-rule),inset -1px 0 0 var(--odr-sheet-line)}
.odr-sheet-row-header{left:0;z-index:1;box-shadow:inset -1px 0 0 var(--odr-sheet-rule),inset 0 -1px 0 var(--odr-sheet-line)}
.odr-sheet-corner{left:0;z-index:3;box-shadow:inset -1px 0 0 var(--odr-sheet-rule),inset 0 -1px 0 var(--odr-sheet-rule)}
.odr-gridlines-soft .odr-sheet td{border-top:1px solid var(--odr-sheet-line);border-left:1px solid var(--odr-sheet-line)}
.odr-gridlines-hard .odr-sheet td{border:1px solid var(--odr-sheet-line)!important}
.odr-sheet td.odr-value-type-float{text-align:right}
/* `background-image` layers over the document's own cell background instead of
   replacing it, and leaves `box-shadow` to the ruler. */
.odr-sheet tbody tr:hover>*{background-image:linear-gradient(var(--odr-sheet-wash),var(--odr-sheet-wash))}
.odr-sheet tbody tr.odr-sheet-pinned>*{background-image:linear-gradient(var(--odr-sheet-wash-pinned),var(--odr-sheet-wash-pinned))}
.odr-sheet tbody tr:hover>th,.odr-sheet tbody tr.odr-sheet-pinned>th{background-image:linear-gradient(var(--odr-sheet-wash-ruler),var(--odr-sheet-wash-ruler))}
.odr-sheet .odr-sheet-pinned-cell{outline:2px solid var(--odr-sheet-focus);outline-offset:-2px}
/* The clipped cell a reader asked to see: out of flow so the row cannot move,
   sized to the string, over its neighbours. `.odr-sheet-raised-box` is the
   wrapper the script adds to a cell that writes its string without one. */
.odr-sheet td.odr-sheet-raised{overflow:visible!important;clip-path:none!important;z-index:4}
.odr-sheet td.odr-sheet-raised.odr-sheet-pinned-cell{outline:none}
.odr-sheet td.odr-sheet-raised>x-p,.odr-sheet td.odr-sheet-raised>.odr-sheet-raised-box{position:absolute!important;left:0;top:0;z-index:4;height:auto!important;min-width:100%;width:max-content;max-width:60vw;padding:1px 6px;margin:-1px -6px;background:var(--odr-sheet-raised)!important;box-shadow:0 1px 4px rgba(0,0,0,.35);outline:2px solid var(--odr-sheet-focus);outline-offset:-2px;overflow:visible!important;white-space:normal!important}
.odr-sheet-raised-box{display:block}
/* The editor is an overlay: over the ruler, which sticks at 3, and over the
   raise at 4. */
.odr-sheet-editor{position:absolute;z-index:5;box-sizing:border-box;margin:0;padding:1px 6px;border:0;outline:2px solid var(--odr-sheet-focus);outline-offset:-2px;border-radius:0;background:var(--odr-sheet-raised)}
.odr-editing td{cursor:cell}
.odr-editing td.odr-locked{cursor:not-allowed}
.odr-sheet td.odr-sheet-refused{outline:2px solid var(--odr-sheet-refused);outline-offset:-2px}
/* The header's `position:sticky` already makes it a containing block. */
.odr-sheet-sort{position:absolute;top:1px;right:1px;bottom:1px;width:17px;display:flex;align-items:center;justify-content:center;border-radius:2px;opacity:0;cursor:pointer}
.odr-sheet-column-header:hover .odr-sheet-sort,.odr-sheet-sort-asc,.odr-sheet-sort-desc{opacity:1}
.odr-sheet-sort:hover{background:var(--odr-sheet-wash-ruler)}
.odr-sheet-sort::after{content:"\25BE";font-size:15px;line-height:1}
.odr-sheet-sort-asc::after{content:"\25B4"}
/* `--odr-print-fit` is written per sheet by `translate_sheet`; `zoom` keeps
   the rows breaking across pages. Only `!important` beats the inline column
   width. The ruler collapses rather than `display:none`, which would take the
   cells out of their rows and shift every column left. */
@media print{
.odr-sheet thead{display:none}
.odr-sheet-gutter{visibility:collapse;width:0}
.odr-sheet-row-header{visibility:collapse;padding:0;box-shadow:none}
.odr-sheet{zoom:var(--odr-print-fit,1);max-width:100%}
.odr-sheet col{min-width:0!important}
}
)css";

constexpr std::string_view spreadsheet_dark_css = R"css(
:root{
--odr-sheet-line:#30363d;
--odr-sheet-rule:#484f58;
--odr-sheet-ruler:#161b22;
--odr-sheet-ruler-text:#8b949e;
--odr-sheet-canvas:#0d1117;
--odr-sheet-wash:rgba(255,255,255,.05);
--odr-sheet-wash-pinned:rgba(255,255,255,.10);
--odr-sheet-wash-ruler:rgba(255,255,255,.12);
--odr-sheet-focus:#4c8dff;
--odr-sheet-refused:#f0665b;
--odr-sheet-raised:#1c2128;
}
.odr-sheet{background-color:#161b22!important}
)css";

/// A whole-pixel line height, because the line numbers are a second column
/// whose cells are sized to the lines by script - a fractional line would round
/// per cell and the two columns would drift apart.
constexpr std::string_view text_css = R"css(
:root{
--odr-text-fg:#1f2328;
--odr-text-muted:#8c959f;
--odr-text-line:#d8dee4;
--odr-text-gutter:#f6f8fa;
--odr-text-wash:rgba(0,0,0,.04);
--odr-text-mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
}
body{margin:0;background:#fff}
.odr-text{display:flex;align-items:stretch;min-height:100vh;color:var(--odr-text-fg);font:13px/20px var(--odr-text-mono);tab-size:4}
/* The numbers are ours, not the file's, so the gutter stays out of a selection
   of the page. */
.odr-text-nr{display:flex;flex-direction:column;flex:none;padding:max(16px,var(--odr-min-margin-top,0px)) 12px max(16px,var(--odr-min-margin-bottom,0px)) max(16px,var(--odr-min-margin-left,0px));text-align:right;color:var(--odr-text-muted);background:var(--odr-text-gutter);border-right:1px solid var(--odr-text-line);font-variant-numeric:tabular-nums;user-select:none;-webkit-user-select:none}
.odr-text-body{display:flex;flex-direction:column;flex:1;min-width:0;padding:max(16px,var(--odr-min-margin-top,0px)) max(16px,var(--odr-min-margin-right,0px)) max(16px,var(--odr-min-margin-bottom,0px)) 16px;white-space:pre}
.odr-text-wrap{white-space:break-spaces;word-break:break-word;overflow-wrap:anywhere}
/* A hovered line reaches its number, which is in the other column, by beginning
   a viewport to the left of it; the padding puts the text back where it was.
   Overflow to the left of the page does not scroll. */
.odr-text-body>div{margin-left:-100vw;padding-left:100vw}
.odr-text-body>div:hover{background:var(--odr-text-wash)}
[contenteditable]:focus{outline:none}
)css";

/// The gutter steps the other way in dark: lighter than the ground.
constexpr std::string_view text_dark_css = R"css(
:root{
color-scheme:dark;
--odr-text-fg:#e6edf3;
--odr-text-muted:#8b949e;
--odr-text-line:#30363d;
--odr-text-gutter:#161b22;
--odr-text-wash:rgba(255,255,255,.05);
}
body{background:#0d1117}
)css";

/// No numbered gutter - the numbers would be ours, not the file's. The column
/// carries the fold handles, and every line reserves it.
constexpr std::string_view xml_css = R"css(
:root{
--odr-xml-text:#1f2328;
--odr-xml-muted:#6e7781;
--odr-xml-punct:#57606a;
--odr-xml-name:#116329;
--odr-xml-attr:#953800;
--odr-xml-value:#0a3069;
--odr-xml-meta:#8250df;
--odr-xml-mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
}
body{margin:0;background:#fff}
.odr-xml{padding:max(16px,var(--odr-min-margin-top,0px)) max(16px,var(--odr-min-margin-right,0px)) max(16px,var(--odr-min-margin-bottom,0px)) max(16px,var(--odr-min-margin-left,0px));color:var(--odr-xml-text);font:13px/1.6 var(--odr-xml-mono);word-break:break-word;overflow-wrap:anywhere}
.odr-xml-line,.odr-xml summary{padding-left:1.5em}
.odr-xml summary{display:block;position:relative;list-style:none;cursor:pointer}
.odr-xml summary::-webkit-details-marker{display:none}
/* U+25BE, U+25B8 */
.odr-xml summary::before{content:"\25BE";position:absolute;left:.35em;color:var(--odr-xml-muted)}
.odr-xml details:not([open])>summary::before{content:"\25B8"}
.odr-xml summary:hover{background:rgba(0,0,0,.04)}
/* Indentation is spaces, not padding, so a copy of the page carries it. */
.odr-xml-indent{white-space:pre}
.odr-xml-tag{color:var(--odr-xml-punct)}
.odr-xml-name{color:var(--odr-xml-name)}
.odr-xml-attr{color:var(--odr-xml-attr)}
.odr-xml-value{color:var(--odr-xml-value)}
/* Source text, everywhere it is written - html would fold a run of spaces. */
.odr-xml-text,.odr-xml-cdata,.odr-xml-comment,.odr-xml-value,.odr-xml-decl,.odr-xml-doctype,.odr-xml-pi{white-space:pre-wrap}
.odr-xml-cdata{color:var(--odr-xml-value)}
.odr-xml-comment{color:var(--odr-xml-muted)}
.odr-xml-decl,.odr-xml-doctype,.odr-xml-pi{color:var(--odr-xml-meta)}
)css";

constexpr std::string_view xml_dark_css = R"css(
:root{
color-scheme:dark;
--odr-xml-text:#e6edf3;
--odr-xml-muted:#8b949e;
--odr-xml-punct:#9198a1;
--odr-xml-name:#7ee787;
--odr-xml-attr:#ffa657;
--odr-xml-value:#a5d6ff;
--odr-xml-meta:#d2a8ff;
}
body{background:#0d1117}
.odr-xml summary:hover{background:rgba(255,255,255,.06)}
)css";

constexpr std::string_view filesystem_css = R"css(
:root{
--odr-files-line:#e3e5e8;
--odr-files-muted:#5c6169;
--odr-files-link:#3c78dc;
--odr-files-font:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;
--odr-files-mono:ui-monospace,SFMono-Regular,Menlo,Consolas,monospace;
}
body{margin:0;padding:var(--odr-min-margin-top,0px) var(--odr-min-margin-right,0px) var(--odr-min-margin-bottom,0px) var(--odr-min-margin-left,0px);background:#fff;color:#1f2328;font:13px/1.5 var(--odr-files-font)}
.odr-files{border-collapse:collapse;width:100%}
.odr-files td{padding:5px 12px;border-top:1px solid var(--odr-files-line)}
.odr-files tbody tr:hover>*{background-image:linear-gradient(rgba(0,0,0,.04),rgba(0,0,0,.04))}
.odr-files-name{font-family:var(--odr-files-mono);word-break:break-all}
.odr-files-name a{color:inherit;text-decoration:none}
.odr-files-name a:hover{color:var(--odr-files-link);text-decoration:underline}
.odr-files-size{width:1%;text-align:right;white-space:nowrap;font-variant-numeric:tabular-nums;color:var(--odr-files-muted)}
.odr-files-action{width:1%;white-space:nowrap;text-align:right}
/* U+2193: the download glyphs are missing from enough system fonts to tofu. */
.odr-files-action a{display:inline-flex;align-items:center;justify-content:center;width:24px;height:24px;border-radius:4px;color:var(--odr-files-muted);font-size:15px;line-height:1;text-decoration:none}
.odr-files-action a:hover{background:rgba(0,0,0,.07);color:var(--odr-files-link)}
)css";

/// The washes lighten rather than darken: a black one paints nothing here.
constexpr std::string_view filesystem_dark_css = R"css(
:root{
color-scheme:dark;
--odr-files-line:#30363d;
--odr-files-muted:#8b949e;
--odr-files-link:#6cb6ff;
}
body{background:#0d1117;color:#e6edf3}
.odr-files tbody tr:hover>*{background-image:linear-gradient(rgba(255,255,255,.05),rgba(255,255,255,.05))}
.odr-files-action a:hover{background:rgba(255,255,255,.08)}
)css";

constexpr std::string_view media_css = R"css(
body{margin:0;background:#000}
.odr-media{display:flex;align-items:center;justify-content:center;min-height:100vh}
.odr-media video{max-width:100%;max-height:100vh}
.odr-media audio{width:100%;max-width:40rem;margin:0 1rem}
)css";

/// What the search script paints.
constexpr std::string_view search_css = R"css(
mark{background:#ff0}
mark.current{background:orange}
)css";

/// The mark keeps its yellow; only the text on it turns over.
constexpr std::string_view search_dark_css = R"css(
mark{color:#0d1117!important}
)css";

constexpr std::string_view document_js = R"js(
(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});

  odr.onError = function (code, message) {
    console.error("error " + code + " message " + message);
  };

  var errorIllegalEditNewLine = {
    code: 1,
    message: "new line not supported by this document",
  };

  var modified = {};

  odr.generateDiff = function () {
    var ops = [];
    for (var path in modified) {
      if (Object.prototype.hasOwnProperty.call(modified, path)) {
        ops.push({ op: "setText", path: path, text: modified[path].innerText });
      }
    }
    return JSON.stringify({ version: 1, ops: ops });
  };

  new MutationObserver(function (mutations) {
    for (var i = 0; i < mutations.length; ++i) {
      if (mutations[i].type !== "characterData") {
        continue;
      }
      // The nearest owner, not the direct parent: a search `<mark>` may sit
      // between the edited text and the element carrying the path.
      var parent = mutations[i].target.parentElement;
      var owner = parent && parent.closest("[data-odr-path]");
      if (owner) {
        modified[owner.getAttribute("data-odr-path")] = owner;
      }
    }
  }).observe(document.body, {
    childList: true,
    subtree: true,
    characterData: true,
  });

  document.addEventListener("keydown", function (event) {
    if (event.key === "Enter") {
      event.preventDefault();
      odr.onError(errorIllegalEditNewLine.code, errorIllegalEditNewLine.message);
    }
  });
})();
)js";

/// The zoom api, and the fit where the css could not state it.
/// `test/browser/viewport/serve` lifts the script out by this declaration read
/// verbatim, so renaming it breaks the browser checks.
constexpr std::string_view viewport_js = R"js(
(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});

  var root = document.documentElement;
  var body = document.body;

  var minZoom = 0.1;
  var maxZoom = 10;

  var framed = window.top !== window.self;

  function declared(name) {
    return getComputedStyle(root).getPropertyValue(name).trim();
  }

  // A number is the css stating the fit; `view` and `auto` ask us to measure
  // it. `auto` only in a frame, where the viewport meta tag is inert.
  var stated = declared("--odr-fit");
  var measures = stated === "view" || (stated === "auto" && framed);
  var fit = measures ? 1 : parseFloat(stated) || 1;

  // `null` while the view follows the fit.
  var pinned = parseFloat(declared("--odr-zoom"));
  if (!isFinite(pinned)) {
    pinned = null;
  }

  // The width the anchor below was taken at: a scroll arriving after the
  // viewport changed is the browser's doing, not the reader's.
  var width = 0;
  // Where the reader is, kept current: by the time a resize arrives the browser
  // has relaid out and moved the scroll.
  var held = null;
  // Our own scrolling, which must not be mistaken for the reader's.
  var restoring = false;
  // Identifies the settling run below, so a newer one - or the reader - ends it.
  var settling = 0;

  function applied() {
    return pinned !== null ? pinned : fit;
  }

  // Webkit divides a stated `text-size-adjust` by the css zoom, so restating
  // the zoom as the percentage holds the type where its box is. Blink reads the
  // same percentage as a plain multiplier, hence the probe below.
  var adjustsText = false;
  var adjusted = "";

  function textAdjust(value) {
    adjusted = value;
    root.style.setProperty("-webkit-text-size-adjust", value);
    root.style.setProperty("text-size-adjust", value);
  }

  // A run against a stated length, so no rect convention enters. True in webkit
  // and in the engines that ignore the property; false where it scales the text
  // a second time.
  function textAdjustHolds() {
    var ruler = document.createElement("div");
    ruler.style.cssText =
      "position:absolute;top:0;left:0;width:400px;height:0;overflow:hidden";
    var run = document.createElement("span");
    run.style.cssText = "font:100px/1 monospace;white-space:pre";
    run.textContent = "MMMMMMMMMM";
    ruler.appendChild(run);
    body.appendChild(ruler);

    function ratio() {
      var length = ruler.getBoundingClientRect().width;
      return length ? run.getBoundingClientRect().width / length : 0;
    }

    var zoom = body.style.zoom;
    body.style.zoom = "1";
    var unzoomed = ratio();
    body.style.zoom = "0.5";
    textAdjust("50%");
    var zoomed = ratio();
    textAdjust("");
    body.style.zoom = zoom;
    body.removeChild(ruler);

    return unzoomed > 0 && Math.abs(zoomed - unzoomed) < unzoomed / 50;
  }

  function zoomBody(zoom) {
    body.style.zoom = zoom;
    root.style.setProperty("--odr-zoom", zoom);
    if (adjustsText) {
      textAdjust(zoom * 100 + "%");
    }
  }

  // The natural width of what the body holds, measured unscaled.
  function contentWidth() {
    var zoom = body.style.zoom;
    var adjust = adjusted;
    // `1`, not empty: a stylesheet may carry a zoom of its own to fall back to.
    body.style.zoom = "1";
    // the percentage compensates a zoom this measurement removes
    if (adjust) {
      textAdjust("100%");
    }
    var natural = body.scrollWidth;
    body.style.zoom = zoom;
    if (adjust) {
      textAdjust(adjust);
    }
    return natural;
  }

  function measureFit() {
    var available = root.clientWidth;
    if (!available) {
      return fit;
    }
    var content = contentWidth();
    if (!content) {
      return fit;
    }
    // Only ever down: a page narrower than the viewport is shown at its size.
    return content > available ? available / content : 1;
  }

  // Whether `getBoundingClientRect` carries the zoom applied to the body;
  // webkit does not. Only decidable while a zoom is applied.
  var rectsZoomed = null;

  // Rect coordinates times this are viewport coordinates.
  function rectFactor() {
    var zoom = parseFloat(getComputedStyle(body).zoom);
    if (!isFinite(zoom) || zoom <= 0 || zoom === 1) {
      return 1;
    }
    if (rectsZoomed === null) {
      var probe = document.createElement("div");
      probe.style.cssText =
        "position:absolute;top:0;left:0;width:100px;height:100px;" +
        "box-sizing:content-box;margin:0;padding:0;border:0";
      body.appendChild(probe);
      var measured = probe.getBoundingClientRect().width;
      body.removeChild(probe);
      if (!measured) {
        return 1;
      }
      rectsZoomed = Math.abs(measured - 100 * zoom) < Math.abs(measured - 100);
    }
    return rectsZoomed ? 1 : zoom;
  }

  // @p element's box in viewport coordinates, shaped like a `DOMRect`.
  function boxOf(element) {
    var box = element.getBoundingClientRect();
    var factor = rectFactor();
    var left = box.left * factor;
    var top = box.top * factor;
    var width = box.width * factor;
    var height = box.height * factor;
    return {
      x: left,
      y: top,
      left: left,
      top: top,
      right: left + width,
      bottom: top + height,
      width: width,
      height: height,
    };
  }

  // The element under @p point, and how far into it that point sits - a
  // fraction of the scroll height cannot stand in, the height scales too. Only
  // a given point pins x; the page column centres itself.
  function anchor(point) {
    var x = point ? point.x : Math.floor(root.clientWidth / 2);
    var y = point ? point.y : 1;
    var element = document.elementFromPoint(x, y);
    if (!element) {
      return null;
    }
    var box = boxOf(element);
    return {
      element: element,
      x: point ? x : null,
      y: y,
      intoX: box.width ? (x - box.left) / box.width : 0,
      intoY: box.height ? (y - box.top) / box.height : 0,
    };
  }

  // `{x, y}`, or the `clientX`/`clientY` of a mouse or touch event.
  function point(value) {
    if (!value) {
      return null;
    }
    var x = value.x !== undefined ? value.x : value.clientX;
    var y = value.y !== undefined ? value.y : value.clientY;
    return isFinite(x) && isFinite(y) ? { x: x, y: y } : null;
  }

  function remember() {
    if (restoring) {
      return;
    }
    if (root.clientWidth !== width) {
      // The viewport changed without a resize event. What is on screen is the
      // browser's guess, not the reader's position, so fit from here instead.
      resized();
      return;
    }
    held = anchor();
  }

  function restore(target) {
    if (!target || !target.element.isConnected) {
      return;
    }
    var box = boxOf(target.element);
    var deltaY = box.top + target.intoY * box.height - target.y;
    var deltaX =
      target.x === null ? 0 : box.left + target.intoX * box.width - target.x;
    if (deltaX || deltaY) {
      window.scrollBy(deltaX, deltaY);
    }
  }

  function notify() {
    if (typeof odr.onZoomChange === "function") {
      odr.onZoomChange(applied(), pinned === null);
    }
  }

  // The browser applies a scroll offset of its own a few frames later, so
  // @p target is re-asserted until it settles.
  function settle(target) {
    restoring = true;
    restore(target);

    var token = ++settling;
    var frames = 30;
    (function again() {
      if (token !== settling) {
        // A newer run - or the reader - owns the state below now.
        return;
      }
      if (frames-- <= 0) {
        restoring = false;
        remember();
        return;
      }
      restore(target);
      requestAnimationFrame(again);
    })();
  }

  function apply(target) {
    zoomBody(applied());

    settle(target);
    notify();
  }

  function resized() {
    if (root.clientWidth === width) {
      // Nothing that changes the scale: a height-only change, or a pinch,
      // where restoring would fight the reader.
      return;
    }

    var target = held;
    width = root.clientWidth;

    if (pinned !== null || !measures) {
      // The scale does not follow the viewport; the reader's place still does.
      settle(target);
      return;
    }

    fit = measureFit();
    apply(target);
  }

  function taken() {
    ++settling;
    restoring = false;
  }

  // `1` is actual size. Excludes the browser's own page and pinch zoom.
  odr.getZoom = function () {
    return applied();
  };

  odr.isZoomFitted = function () {
    return pinned === null;
  };

  // @p element's box in the coordinates `elementFromPoint` takes, for a host
  // hit-testing while a zoom is applied.
  odr.getViewportRect = function (element) {
    return element && typeof element.getBoundingClientRect === "function"
      ? boxOf(element)
      : null;
  };

  // @p focus, a pinch's midpoint, is the point that stays put across the
  // change; the top of the viewport where none is given.
  odr.setZoom = function (value, focus) {
    var next = Number(value);
    if (!isFinite(next)) {
      return applied();
    }
    pinned = Math.min(maxZoom, Math.max(minZoom, next));
    // read now, unlike a resize, which arrives relaid out
    apply(anchor(point(focus)));
    return applied();
  };

  odr.adjustZoom = function (factor, focus) {
    return odr.setZoom(applied() * Number(factor), focus);
  };

  odr.resetZoom = function (focus) {
    pinned = null;
    var target = anchor(point(focus));
    if (measures) {
      fit = measureFit();
    }
    apply(target);
    return applied();
  };

  // Before the first measurement, so every one of them reads the same state.
  adjustsText = textAdjustHolds();
  width = root.clientWidth;
  if (measures && pinned === null) {
    fit = measureFit();
  }
  // Restated inline so a css-stated zoom carries the adjustment with it.
  if (applied() !== 1) {
    zoomBody(applied());
  }
  remember();

  window.addEventListener("scroll", remember, { passive: true });
  window.addEventListener("resize", resized);
  if (window.visualViewport) {
    window.visualViewport.addEventListener("resize", resized);
  }
  // Anything the reader does ends the re-assertion above.
  window.addEventListener("wheel", taken, { passive: true });
  window.addEventListener("touchstart", taken, { passive: true });
  window.addEventListener("keydown", taken);
})();
)js";

/// The annotation overlays, and what `setOptions` writes into css.
constexpr std::string_view pdf_annotation_css = R"css(
.an{position:absolute;inset:0;overflow:visible;pointer-events:none;z-index:3}
/* on the overlay, not on the shape: a blend inside an svg composites against
   the svg's own canvas, so the highlight would paint over the glyphs */
.an-m{mix-blend-mode:multiply}
/* the two properties `setOptions` writes */
.p.an-draw .an{pointer-events:auto;cursor:crosshair;touch-action:var(--odr-an-touch,none)}
.p.an-draw .t,.p.an-draw .sel{pointer-events:none}
/* neither may interrupt a stroke */
.p.an-draw{-webkit-touch-callout:none;-webkit-user-select:none;user-select:none}
html.an-drawing{overscroll-behavior:var(--odr-an-overscroll,contain)}
)css";
/// `odr.annotation`: the pending markup a viewer draws, and the payload
/// `PdfFile::annotate` takes. Geometry is kept in page-box points and mapped
/// to pdf user space only on the way out, through the `data-odr-space` each
/// page carries.
constexpr std::string_view pdf_annotation_js = R"js(
(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});
  var SVG = "http://www.w3.org/2000/svg";

  var tool = null;
  var color = [1, 0.9, 0.2];
  var width = 2;
  var pending = [];
  var nextId = 1;

  /// Gesture policy, the viewer's to set. `inkPointerTypes` null takes any.
  var options = {
    markOnSelection: false,
    inkPointerTypes: null,
    touchAction: "none",
    overscrollBehavior: "contain",
  };

  function pages() {
    return Array.prototype.slice.call(
      document.querySelectorAll("[data-odr-space]")
    );
  }

  function pageOf(index) {
    var all = pages();
    for (var i = 0; i < all.length; ++i) {
      if (+all[i].getAttribute("data-odr-page") === index) {
        return all[i];
      }
    }
    return null;
  }

  /// A viewport point to page-box points (y-down, the unit the overlay draws
  /// in). The page box is laid out in inches, so its own layout width in css
  /// pixels gives the scale a zoom transform is applied on top of.
  function toBox(page, clientX, clientY) {
    var rect = page.getBoundingClientRect();
    var zoom = page.offsetWidth ? rect.width / page.offsetWidth : 1;
    return [
      ((clientX - rect.left) / zoom) * 0.75,
      ((clientY - rect.top) / zoom) * 0.75,
    ];
  }

  /// Page-box points to pdf user space, through the page's own inverse.
  function toUserSpace(page, x, y) {
    var m = page.getAttribute("data-odr-space").split(",").map(Number);
    return [
      m[0] * x + m[2] * y + m[4],
      m[1] * x + m[3] * y + m[5],
    ];
  }

  /// Two overlays per page: `multiply` for the washes that have to let the
  /// text through, and a normal one for the marks drawn on top of it.
  function overlay(page, multiply) {
    var name = multiply ? "an an-m" : "an";
    var svg = page.querySelector(
      ':scope > svg[class="' + name + '"]'
    );
    if (!svg) {
      svg = document.createElementNS(SVG, "svg");
      svg.setAttribute("class", name);
      svg.setAttribute("preserveAspectRatio", "none");
      page.appendChild(svg);
    }
    svg.setAttribute(
      "viewBox",
      "0 0 " + page.offsetWidth * 0.75 + " " + page.offsetHeight * 0.75
    );
    return svg;
  }

  function css(c) {
    return (
      "rgb(" +
      c
        .map(function (v) {
          return Math.round(Math.max(0, Math.min(1, v)) * 255);
        })
        .join(",") +
      ")"
    );
  }

  function inkPath(strokes) {
    return strokes
      .map(function (s) {
        var d = "M " + s[0] + " " + s[1];
        for (var i = 2; i < s.length; i += 2) {
          d += " L " + s[i] + " " + s[i + 1];
        }
        return d;
      })
      .join(" ");
  }

  function draw(annotation) {
    var page = pageOf(annotation.page);
    if (!page) {
      return null;
    }
    var svg = overlay(page, annotation.type === "highlight");
    var node = document.createElementNS(SVG, "path");
    if (annotation.type === "ink") {
      node.setAttribute("d", inkPath(annotation.strokes));
      node.setAttribute("fill", "none");
      node.setAttribute("stroke", css(annotation.color));
      node.setAttribute("stroke-width", annotation.width);
      node.setAttribute("stroke-linecap", "round");
      node.setAttribute("stroke-linejoin", "round");
    } else {
      node.setAttribute("d", annotation.boxes.map(barPath(annotation.type)).join(" "));
      if (annotation.type === "squiggly") {
        node.setAttribute("fill", "none");
        node.setAttribute("stroke", css(annotation.color));
        node.setAttribute("stroke-width", 1);
      } else {
        node.setAttribute("fill", css(annotation.color));
      }
    }
    node.setAttribute("data-odr-annotation", annotation.id);
    svg.appendChild(node);
    return node;
  }

  /// The shape one covered box gets, in page-box points.
  function barPath(type) {
    return function (b) {
      var h = b[3] - b[1];
      if (type === "highlight") {
        return rect(b[0], b[1], b[2] - b[0], h);
      }
      if (type === "underline") {
        return rect(b[0], b[3] - h / 16, b[2] - b[0], Math.max(h / 16, 0.5));
      }
      if (type === "strikeOut") {
        return rect(b[0], b[1] + h / 2, b[2] - b[0], Math.max(h / 16, 0.5));
      }
      var step = Math.max(h / 8, 1);
      var d = "M " + b[0] + " " + (b[3] - step);
      var up = true;
      for (var x = b[0] + step; x < b[2]; x += step, up = !up) {
        d += " L " + x + " " + (up ? b[3] - step * 2 : b[3] - step);
      }
      return d;
    };
  }

  function rect(x, y, w, h) {
    return "M " + x + " " + y + " h " + w + " v " + h + " h " + -w + " Z";
  }

  function redraw() {
    pages().forEach(function (page) {
      page.querySelectorAll(":scope > svg.an").forEach(function (svg) {
        svg.textContent = "";
      });
    });
    pending.forEach(draw);
    // a rebuild throws away the node a live stroke draws into
    strokeNode = stroke
      ? document.querySelector('[data-odr-annotation="' + stroke.id + '"]')
      : null;
  }

  function pushBox(byPage, left, top, right, bottom) {
    if (right - left < 0.5 || bottom - top < 0.5) {
      return;
    }
    var page = pageAt((left + right) / 2, (top + bottom) / 2);
    if (!page) {
      return;
    }
    var index = +page.getAttribute("data-odr-page");
    var a = toBox(page, left, top);
    var b = toBox(page, right, bottom);
    (byPage[index] = byPage[index] || []).push([a[0], a[1], b[0], b[1]]);
  }

  /// The selection-layer runs a range touches; a spacer carries no text.
  function selectedRuns(selection, range) {
    var scope = range.commonAncestorContainer;
    if (!scope.querySelectorAll) {
      scope = scope.parentElement;
    }
    if (!scope) {
      return [];
    }
    var self = scope.closest ? scope.closest(".sr") : null;
    if (self) {
      return self.textContent.length > 0 ? [self] : [];
    }
    return Array.prototype.filter.call(
      scope.querySelectorAll(".sr"),
      function (run) {
        return run.textContent.length > 0 && selection.containsNode(run, true);
      }
    );
  }

  /// One run's covered box; a partly selected run takes its horizontal edges
  /// from the rects, clamped to the run.
  function runBox(byPage, run, rects, selection) {
    var box = run.getBoundingClientRect();
    var left = box.left;
    var right = box.right;
    if (!selection.containsNode(run, false)) {
      left = Infinity;
      right = -Infinity;
      for (var i = 0; i < rects.length; ++i) {
        var rect = rects[i];
        if (
          rect.width < 0.5 ||
          rect.bottom <= box.top ||
          rect.top >= box.bottom ||
          rect.right <= box.left ||
          rect.left >= box.right
        ) {
          continue;
        }
        left = Math.min(left, Math.max(rect.left, box.left));
        right = Math.max(right, Math.min(rect.right, box.right));
      }
    }
    pushBox(byPage, left, box.top, right, box.bottom);
  }

  /// The boxes a selection covers, per page, in page-box points. Vertically
  /// the run's box, not the range's rect: that rect follows whatever font the
  /// browser substituted for the layer.
  function selectionBoxes() {
    var selection = window.getSelection();
    var byPage = {};
    if (!selection || selection.isCollapsed) {
      return byPage;
    }
    for (var r = 0; r < selection.rangeCount; ++r) {
      var range = selection.getRangeAt(r);
      var rects = range.getClientRects();
      var runs = selectedRuns(selection, range);
      for (var i = 0; i < runs.length; ++i) {
        runBox(byPage, runs[i], rects, selection);
      }
      if (runs.length === 0) {
        // no selection layer under it
        for (var k = 0; k < rects.length; ++k) {
          pushBox(byPage, rects[k].left, rects[k].top, rects[k].right, rects[k].bottom);
        }
      }
    }
    return byPage;
  }

  function pageAt(x, y) {
    var all = pages();
    for (var i = 0; i < all.length; ++i) {
      var rect = all[i].getBoundingClientRect();
      if (x >= rect.left && x <= rect.right && y >= rect.top && y <= rect.bottom) {
        return all[i];
      }
    }
    return null;
  }

  /// One annotation per page the selection covers. `keep` holds the selection,
  /// which the automatic path cannot: the next `selectionchange` re-marks it.
  function markSelection(keep) {
    if (!tool || tool === "ink") {
      return false;
    }
    var byPage = selectionBoxes();
    var added = false;
    Object.keys(byPage).forEach(function (index) {
      pending.push({
        id: nextId++,
        page: +index,
        type: tool,
        boxes: byPage[index],
        color: color.slice(),
      });
      added = true;
    });
    if (added) {
      if (!keep) {
        window.getSelection().removeAllRanges();
      }
      redraw();
    }
    return added;
  }

)js";
/// The rest of it; see `Asset::content_tail`.
constexpr std::string_view pdf_annotation_js_tail = R"js(
  var stroke = null;
  var strokeNode = null;
  var strokePointer = null;
  var strokeData = "";
  var strokeFrame = 0;
  var pointerDown = false;
  var settle = null;

  /// One dom write per frame; a pen reports faster than the page paints.
  function flushStroke() {
    strokeFrame = 0;
    if (strokeNode) {
      strokeNode.setAttribute("d", strokeData);
    }
  }

  function scheduleFlush() {
    if (!strokeFrame) {
      strokeFrame = window.requestAnimationFrame(flushStroke);
    }
  }

  /// A drag fires `selectionchange` on every character it covers, so the mark
  /// waits for the gesture that makes it to end rather than taking the first
  /// character and tearing the selection out from under the pointer.
  function scheduleMark() {
    if (!options.markOnSelection || !tool || tool === "ink" || pointerDown) {
      return;
    }
    window.clearTimeout(settle);
    settle = window.setTimeout(function () {
      markSelection(false);
    }, 50);
  }

  function inkTakes(event) {
    return (
      options.inkPointerTypes === null ||
      options.inkPointerTypes.indexOf(event.pointerType) !== -1
    );
  }

  function onPointerDown(event) {
    pointerDown = true;
    // a new gesture supersedes a mark the previous one had queued
    window.clearTimeout(settle);
    if (tool !== "ink" || event.button !== 0 || !inkTakes(event)) {
      return;
    }
    var page = pageAt(event.clientX, event.clientY);
    if (!page) {
      return;
    }
    event.preventDefault();
    var p = toBox(page, event.clientX, event.clientY);
    stroke = {
      id: nextId++,
      page: +page.getAttribute("data-odr-page"),
      type: "ink",
      strokes: [[p[0], p[1]]],
      color: color.slice(),
      width: width,
    };
    pending.push(stroke);
    strokePointer = event.pointerId;
    strokeData = "M " + p[0] + " " + p[1];
    strokeNode = draw(stroke);
    page.setPointerCapture(event.pointerId);
  }

  function onPointerMove(event) {
    if (!stroke || event.pointerId !== strokePointer) {
      return;
    }
    var page = pageOf(stroke.page);
    var points = stroke.strokes[0];
    // a synthetic event coalesces none, and is its own sample
    var samples =
      typeof event.getCoalescedEvents === "function"
        ? event.getCoalescedEvents()
        : [];
    if (samples.length === 0) {
      samples = [event];
    }
    var appended = false;
    for (var i = 0; i < samples.length; ++i) {
      var p = toBox(page, samples[i].clientX, samples[i].clientY);
      // drop the sub-point jitter a pointer emits while nearly still
      if (
        Math.abs(p[0] - points[points.length - 2]) +
          Math.abs(p[1] - points[points.length - 1]) <
        0.5
      ) {
        continue;
      }
      points.push(p[0], p[1]);
      strokeData += " L " + p[0] + " " + p[1];
      appended = true;
    }
    if (appended) {
      scheduleFlush();
    }
  }

  function onPointerUp(event) {
    pointerDown = false;
    scheduleMark();
    if (!stroke || (event && event.pointerId !== strokePointer)) {
      return;
    }
    var points = stroke.strokes[0];
    if (points.length < 4) {
      // a tap with no drag leaves a dot, which is a legitimate mark
      points.push(points[0], points[1]);
      strokeData += " L " + points[0] + " " + points[1];
    }
    flushStroke();
    stroke = null;
    strokeNode = null;
    strokePointer = null;
  }

  function applyOptions() {
    var style = document.documentElement.style;
    style.setProperty("--odr-an-touch", options.touchAction);
    style.setProperty("--odr-an-overscroll", options.overscrollBehavior);
  }

  document.addEventListener("pointerdown", onPointerDown);
  document.addEventListener("pointermove", onPointerMove);
  document.addEventListener("pointerup", onPointerUp);
  document.addEventListener("pointercancel", onPointerUp);
  document.addEventListener("selectionchange", scheduleMark);
  window.addEventListener("resize", redraw);
  applyOptions();

  odr.annotation = {
    /// null, "highlight", "underline", "strikeOut", "squiggly" or "ink".
    setTool: function (value) {
      tool = value || null;
      pages().forEach(function (page) {
        page.classList.toggle("an-draw", tool === "ink");
      });
      document.documentElement.classList.toggle("an-drawing", tool === "ink");
    },
    getTool: function () {
      return tool;
    },
    /// DeviceRGB, each component in [0, 1].
    setColor: function (value) {
      color = value.slice(0, 3).map(Number);
    },
    setWidth: function (value) {
      width = Number(value);
    },
    /// Merged into what is set; an unknown key throws.
    setOptions: function (value) {
      Object.keys(value || {}).forEach(function (key) {
        if (!Object.prototype.hasOwnProperty.call(options, key)) {
          throw new Error("odr.annotation: unknown option " + key);
        }
        options[key] = value[key];
      });
      applyOptions();
    },
    getOptions: function () {
      var copy = {};
      Object.keys(options).forEach(function (key) {
        copy[key] = options[key];
      });
      return copy;
    },
    /// Marks the selection with the armed tool, and answers whether anything
    /// was added. The selection is left standing.
    mark: function () {
      return markSelection(true);
    },
    /// What is pending, newest last. Geometry is in page-box points.
    list: function () {
      return pending.slice();
    },
    remove: function (id) {
      pending = pending.filter(function (a) {
        return a.id !== id;
      });
      redraw();
    },
    undo: function () {
      pending.pop();
      redraw();
    },
    clear: function () {
      pending = [];
      redraw();
    },
    /// The payload `PdfFile::annotate` takes, in pdf user space.
    getAnnotations: function () {
      return JSON.stringify({
        version: 1,
        annotations: pending.map(function (a) {
          var page = pageOf(a.page);
          if (a.type === "ink") {
            return {
              page: a.page,
              type: "ink",
              strokes: a.strokes.map(function (s) {
                var out = [];
                for (var i = 0; i < s.length; i += 2) {
                  var p = toUserSpace(page, s[i], s[i + 1]);
                  out.push(p[0], p[1]);
                }
                return out;
              }),
              width: a.width,
              color: a.color,
            };
          }
          return {
            page: a.page,
            type: a.type,
            quads: a.boxes.map(function (b) {
              // upper-left, upper-right, lower-left, lower-right
              var ul = toUserSpace(page, b[0], b[1]);
              var ur = toUserSpace(page, b[2], b[1]);
              var ll = toUserSpace(page, b[0], b[3]);
              var lr = toUserSpace(page, b[2], b[3]);
              return [ul[0], ul[1], ur[0], ur[1], ll[0], ll[1], lr[0], lr[1]];
            }),
            color: a.color,
          };
        }),
      });
    },
  };
})();
)js";

/// Text search over the rendered page, format-agnostic: it walks text nodes.
constexpr std::string_view search_js = R"js(
(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});

  var marks = [];
  var current = -1;
  var keyword = "";

  // Case- and diacritic-folded `text` plus a folded-index to source-index map
  // (with an end sentinel), so a match maps back onto the source string.
  // Folding per character is what keeps that map right when a character folds
  // to none or to several. Every space folds to one: a run's leading and
  // trailing space is written as `&nbsp;` and a tab as `&emsp;`, and a keyword
  // is typed with neither.
  function fold(text) {
    var folded = "";
    var map = [];
    for (var i = 0; i < text.length; ++i) {
      var character = text[i]
        .normalize("NFD")
        .replace(/[\u0300-\u036f]/g, "")
        .replace(/[\u00a0\u2000-\u200a\u202f\u205f\u3000]/g, " ")
        .toLowerCase();
      for (var j = 0; j < character.length; ++j) {
        map.push(i);
      }
      folded += character;
    }
    map.push(text.length);
    return { text: folded, map: map };
  }

  var inlineElements = new WeakMap();

  function isInline(element) {
    var inline = inlineElements.get(element);
    if (inline === undefined) {
      var display = getComputedStyle(element).display;
      inline = display.indexOf("inline") === 0 || display === "contents";
      inlineElements.set(element, inline);
    }
    return inline;
  }

  // The box a text node flows in. Text under one reads as a run; text under two
  // does not, and a keyword must not match across the break between them.
  function blockOf(node) {
    var element = node.parentElement;
    while (element !== null && isInline(element)) {
      element = element.parentElement;
    }
    return element;
  }

  // Rejected by the subtree, `aria-hidden` included: that is what keeps a pdf's
  // glyph layer out of a search of the same page's text layer.
  function textNodes() {
    var walker = document.createTreeWalker(
      document.body,
      NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT,
      {
        acceptNode: function (node) {
          if (node.nodeType !== Node.TEXT_NODE) {
            var name = node.nodeName;
            return name === "SCRIPT" ||
              name === "STYLE" ||
              name === "MARK" ||
              node.getAttribute("aria-hidden") === "true"
              ? NodeFilter.FILTER_REJECT
              : NodeFilter.FILTER_SKIP;
          }
          return node.nodeValue.length > 0
            ? NodeFilter.FILTER_ACCEPT
            : NodeFilter.FILTER_REJECT;
        },
      }
    );
    var nodes = [];
    while (walker.nextNode()) {
      nodes.push(walker.currentNode);
    }
    return nodes;
  }

  // One folded string per block, with the piece of it each text node
  // contributed. A pdf writes a word per span and a slide a run per span, so a
  // keyword spanning several of them is the ordinary case, not the exception.
  function blocks() {
    var nodes = textNodes();
    var result = [];
    var block = null;
    var open = null;
    for (var i = 0; i < nodes.length; ++i) {
      var node = nodes[i];
      var owner = blockOf(node);
      if (open === null || owner !== block) {
        open = { text: "", pieces: [] };
        block = owner;
        result.push(open);
      }
      var folded = fold(node.nodeValue);
      open.pieces.push({
        node: node,
        begin: open.text.length,
        end: open.text.length + folded.text.length,
        map: folded.map,
      });
      open.text += folded.text;
    }
    return result;
  }

  // Wraps `[from, to)` of a text node. Applied back to front within a node, so
  // the split never moves an offset still to be used.
  function wrap(node, from, to) {
    var match = node.splitText(from);
    match.splitText(to - from);
    var mark = document.createElement("mark");
    mark.className = "highlight";
    match.parentNode.replaceChild(mark, match);
    mark.appendChild(match);
    return mark;
  }

  // Every occurrence in `block`, each as the slices of the text nodes it
  // covers. Collected before anything is wrapped, because wrapping splits the
  // nodes the offsets are measured in.
  function findIn(block, needle) {
    var found = [];
    var at = block.text.indexOf(needle);
    while (at !== -1) {
      var end = at + needle.length;
      var slices = [];
      for (var i = 0; i < block.pieces.length; ++i) {
        var piece = block.pieces[i];
        if (piece.end <= at || piece.begin >= end) {
          continue;
        }
        slices.push({
          node: piece.node,
          from: piece.map[Math.max(at, piece.begin) - piece.begin],
          to: piece.map[Math.min(end, piece.end) - piece.begin],
        });
      }
      if (slices.length > 0) {
        found.push(slices);
      }
      at = block.text.indexOf(needle, end);
    }
    return found;
  }

  function select(index) {
    if (current >= 0 && marks[current]) {
      marks[current].forEach(function (mark) {
        mark.classList.remove("current");
      });
    }
    current = index;
    marks[current].forEach(function (mark) {
      mark.classList.add("current");
    });
    // A hit inside a folded section is scrolled to but not shown.
    for (
      var element = marks[current][0].parentElement;
      element !== null;
      element = element.parentElement
    ) {
      if (element.nodeName === "DETAILS") {
        element.open = true;
      }
    }
    marks[current][0].scrollIntoView({ block: "center", inline: "center" });
  }

  function step(delta, next) {
    if (next !== undefined && next !== null && fold(String(next)).text !== keyword) {
      return odr.search(next);
    }
    if (marks.length === 0) {
      return 0;
    }
    select((current + delta + marks.length) % marks.length);
    return marks.length;
  }

  odr.resetSearch = function () {
    marks.forEach(function (hit) {
      hit.forEach(function (mark) {
        var parent = mark.parentNode;
        if (!parent) {
          return;
        }
        while (mark.firstChild) {
          parent.insertBefore(mark.firstChild, mark);
        }
        parent.removeChild(mark);
        parent.normalize();
      });
    });
    marks = [];
    current = -1;
    keyword = "";
  };

  // Highlights every occurrence, selects the first and returns the count. An
  // occurrence is one hit however many nodes it is written across.
  odr.search = function (text) {
    odr.resetSearch();
    keyword = fold(text === undefined || text === null ? "" : String(text)).text;
    if (keyword === "") {
      return 0;
    }
    blocks().forEach(function (block) {
      var hits = findIn(block, keyword);
      // Wrapped back to front: an offset still to be used sits before the split
      // that would move it. The marks are collected back into reading order.
      var wrapped = [];
      for (var i = hits.length - 1; i >= 0; --i) {
        var hit = [];
        for (var j = hits[i].length - 1; j >= 0; --j) {
          var slice = hits[i][j];
          hit.unshift(wrap(slice.node, slice.from, slice.to));
        }
        wrapped.unshift(hit);
      }
      marks = marks.concat(wrapped);
    });
    if (marks.length > 0) {
      select(0);
    }
    return marks.length;
  };

  // An argument is searched for first unless it is already the highlighted
  // keyword, so a host can drive search and step from the same string.
  odr.searchNext = function (text) {
    return step(1, text);
  };

  odr.searchPrevious = function (text) {
    return step(-1, text);
  };
})();
)js";

/// Highlights the row and column under the pointer, and pins them on a click.
/// A column has no `:hover` selector, so one generated `:nth-child` rule lights
/// it — free per cell, but only correct while cell and column line up.
constexpr std::string_view spreadsheet_js = R"js(
(function () {
  "use strict";

  var table = document.querySelector(".odr-sheet");
  if (table === null) {
    return;
  }

  var merged = table.querySelector("td[colspan],td[rowspan]") !== null;

  var odr = (window.odr = window.odr || {});

  var style = document.createElement("style");
  document.head.appendChild(style);

  var hovered = -1;
  var pinnedColumn = -1;
  var pinnedRow = null;
  var pinnedCell = null;

  // Column 0 is the gutter, which labels no column.
  function columnRule(index, wash, scope) {
    if (index < 1) {
      return "";
    }
    return (
      ".odr-sheet " +
      scope +
      "tr>:nth-child(" +
      (index + 1) +
      "){background-image:linear-gradient(" +
      wash +
      "," +
      wash +
      ")}"
    );
  }

  // The ruler reacts harder than the cells: it is the label being followed.
  function paint() {
    style.textContent =
      columnRule(hovered, "var(--odr-sheet-wash)", "") +
      columnRule(pinnedColumn, "var(--odr-sheet-wash-pinned)", "") +
      columnRule(hovered, "var(--odr-sheet-wash-ruler)", "thead ") +
      columnRule(pinnedColumn, "var(--odr-sheet-wash-ruler)", "thead ");
  }

  // What the wash paints: the cell's place among the ones written beside it,
  // gutter included, which is what `nth-child` counts. Not a position - a
  // merge writes nothing for a covered one, and gets no wash either.
  function rulerColumn(cell) {
    return cell !== null && !merged ? cell.cellIndex : -1;
  }

  // The gutter's label, which names the row wherever a sort has put it.
  function rowOf(tr) {
    return Number(tr.cells[0].textContent) - 1;
  }

  var index = null;

  // Whether a cell above still reaches into @p row.
  function holds(above, row) {
    return above !== undefined && above.last >= row;
  }

  // A row by its label and, where the sheet merges, its cells by position.
  // Walked once: a merged sheet is offered no sort control, so the rows are
  // still in the file's order here and one pass can carry the rowspans down.
  function build() {
    var index = { rows: new Map(), positions: new Map() };
    var covered = [];
    var body = table.tBodies[0];
    for (var i = 0; i < body.rows.length; ++i) {
      var tr = body.rows[i];
      var row = rowOf(tr);
      var line = [];
      index.rows.set(row, { tr: tr, cells: line });
      if (!merged) {
        continue;
      }
      var column = 0;
      for (var j = 1; j < tr.cells.length; ++j) {
        var td = tr.cells[j];
        while (holds(covered[column], row)) {
          line[column] = covered[column].cell;
          ++column;
        }
        var columns = Number(td.getAttribute("colspan") || 1);
        var last = row + Number(td.getAttribute("rowspan") || 1) - 1;
        for (var k = 0; k < columns; ++k) {
          line[column + k] = td;
          covered[column + k] = { last: last, cell: td };
        }
        index.positions.set(td, { column: column, row: row });
        column += columns;
      }
      // A rowspan reaching past the row's last cell covers the rest of it.
      for (; column < covered.length; ++column) {
        if (holds(covered[column], row)) {
          line[column] = covered[column].cell;
        }
      }
    }
    return index;
  }

  function indexed() {
    if (index === null) {
      index = build();
    }
    return index;
  }

  // The `td` at a position, or null past the sheet's extent. A position a
  // merge covers answers with the cell covering it, which is the one the file
  // states and an op names.
  function cellAt(column, row) {
    var entry = indexed().rows.get(row);
    if (entry === undefined || column < 0) {
      return null;
    }
    var cell = merged ? entry.cells[column] : entry.tr.cells[column + 1];
    return cell === undefined ? null : cell;
  }

  // Where a `td` sits, the way an op names it. Null for anything else - a
  // header, a cell of another table.
  function positionOf(cell) {
    if (cell === null || cell.tagName !== "TD") {
      return null;
    }
    if (merged) {
      var position = indexed().positions.get(cell);
      return position === undefined
        ? null
        : { column: position.column, row: position.row };
    }
    return { column: cell.cellIndex - 1, row: rowOf(cell.parentElement) };
  }

  var raisedCell = null;
  var raisedWrapper = null;
  var raisedContent = null;

  // The block the cell writes, or the cell where it writes none. `null` for
  // anything else — a shape, several blocks — which is not raised.
  function boxOf(cell) {
    if (cell.childElementCount === 0) {
      return cell;
    }
    var only = cell.firstElementChild;
    return cell.childElementCount === 1 && only.tagName === "X-P" ? only : null;
  }

  // Past the cell's edge by the spill `translate_sheet` measured, at the edge
  // where it clips, unbounded where it does neither.
  function visibleRight(cell) {
    var style = getComputedStyle(cell);
    var right = cell.getBoundingClientRect().right;
    var inset = /inset\(([^)]*)\)/.exec(style.clipPath || "");
    if (inset !== null) {
      var sides = inset[1].trim().split(/\s+/);
      return sides.length > 1 ? right - parseFloat(sides[1]) : right;
    }
    return style.overflow === "visible" ? Infinity : right;
  }

  // On the text, not the box: what is cut off is the string running past where
  // the cell still paints.
  function cutOff(cell, box) {
    var range = document.createRange();
    range.selectNodeContents(box);
    var ink = range.getBoundingClientRect();
    var rect = cell.getBoundingClientRect();
    return (
      ink.width > 0 &&
      (ink.right > visibleRight(cell) + 1 ||
        (getComputedStyle(cell).overflow !== "visible" &&
          ink.bottom > rect.bottom + 1))
    );
  }

  function lower() {
    if (raisedCell === null) {
      return;
    }
    raisedCell.classList.remove("odr-sheet-raised");
    if (raisedWrapper !== null) {
      while (raisedWrapper.firstChild) {
        raisedCell.insertBefore(raisedWrapper.firstChild, raisedWrapper);
      }
      raisedWrapper.remove();
      raisedWrapper = null;
    }
    raisedContent = null;
    raisedCell = null;
  }

  // Over its neighbours rather than pushing them aside.
  function raise(cell) {
    lower();
    if (cell === null || cell.tagName !== "TD") {
      return;
    }
    var box = boxOf(cell);
    if (box === null || !cutOff(cell, box)) {
      return;
    }
    if (box === cell) {
      raisedWrapper = document.createElement("span");
      raisedWrapper.className = "odr-sheet-raised-box";
      while (cell.firstChild) {
        raisedWrapper.appendChild(cell.firstChild);
      }
      cell.appendChild(raisedWrapper);
      box = raisedWrapper;
    }
    cell.classList.add("odr-sheet-raised");
    raisedCell = cell;
    raisedContent = box;
  }

  function pin(column, row, cell) {
    lower();
    if (pinnedRow !== null) {
      pinnedRow.classList.remove("odr-sheet-pinned");
    }
    if (pinnedCell !== null) {
      pinnedCell.classList.remove("odr-sheet-pinned-cell");
    }

    pinnedColumn = column;
    pinnedRow = row;
    pinnedCell = cell;

    if (pinnedRow !== null) {
      pinnedRow.classList.add("odr-sheet-pinned");
    }
    if (pinnedCell !== null) {
      pinnedCell.classList.add("odr-sheet-pinned-cell");
      raise(pinnedCell);
    }
    paint();
  }

  // What is pinned: a cell, or a whole column or row where a header is, the
  // axis that header does not name being null. Null where nothing is pinned.
  function pinnedPosition() {
    if (pinnedCell === null) {
      return null;
    }
    var position = positionOf(pinnedCell);
    if (position !== null) {
      return { column: position.column, row: position.row, cell: pinnedCell };
    }
    return {
      column: pinnedCell.classList.contains("odr-sheet-column-header")
        ? pinnedCell.cellIndex - 1
        : null,
      row: pinnedRow === null ? null : rowOf(pinnedRow),
      cell: pinnedCell,
    };
  }

  // Pins the cell at a position, as a click on it does; null clears the pin.
  // False where the sheet holds no such cell.
  function pinAt(position) {
    if (position === null) {
      pin(-1, null, null);
      return true;
    }
    var cell = cellAt(position.column, position.row);
    if (cell === null) {
      return false;
    }
    pin(rulerColumn(cell), cell.parentElement, cell);
    return true;
  }

  // Nothing a reader would see, so the cell beside it may spill over it.
  function isBlank(cell) {
    return (
      cell.textContent.trim() === "" &&
      cell.querySelector(":not(x-p):not(x-s)") === null
    );
  }

  // A row's cells by position, a covered one answering with the cell covering
  // it. Null past the sheet's last row.
  function rowCells(row) {
    var entry = indexed().rows.get(row);
    if (entry === undefined) {
      return null;
    }
    if (merged) {
      return entry.cells;
    }
    return Array.prototype.slice.call(entry.tr.cells, 1);
  }

  // The row's cells once each, with what `translate_sheet` states about them:
  // `max-width:0` where the column states a width, which is where it also
  // clips, and `nowrap` where the string may run past the cell.
  function rowState(row) {
    var cells = rowCells(row);
    if (cells === null) {
      return null;
    }
    var line = [];
    for (var i = 0; i < cells.length; ++i) {
      if (cells[i] === cells[i - 1]) {
        continue;
      }
      var style = getComputedStyle(cells[i]);
      line.push({
        cell: cells[i],
        blank: isBlank(cells[i]),
        sized: style.maxWidth === "0px",
        nowrap: style.whiteSpace === "nowrap",
      });
    }
    return line;
  }

  // The spill `translate_sheet` measured goes stale the moment a cell fills or
  // empties: its rule again, off the geometry the browser has. Offsets, not
  // rects: blink scales a rect by the body zoom, a `clip-path` is stated under
  // it.
  function reflow(row) {
    var line = rowState(row);
    if (line === null) {
      return false;
    }

    // What each cell sees to its right: the next one showing something, or
    // the column stating no width that stops the spill before one.
    var bound = null;
    var stopped = false;
    for (var i = line.length - 1; i >= 0; --i) {
      line[i].bound = bound;
      line[i].stopped = stopped;
      if (!line[i].blank) {
        bound = line[i].cell;
        stopped = false;
      } else if (!line[i].sized) {
        bound = null;
        stopped = true;
      }
    }

    for (var j = 0; j < line.length; ++j) {
      var entry = line[j];
      if (!entry.sized || !entry.nowrap) {
        continue;
      }
      var spill =
        entry.bound === null
          ? 0
          : entry.bound.offsetLeft -
            entry.cell.offsetLeft -
            entry.cell.offsetWidth;
      entry.cell.style.overflow =
        spill > 0.5 || (entry.bound === null && !entry.stopped)
          ? "visible"
          : "hidden";
      entry.cell.style.clipPath =
        spill > 0.5 ? "inset(0 " + -spill + "px 0 0)" : "none";
    }
    return true;
  }

  // The run a write goes through, so its style survives; the cell itself
  // where it writes its string without one.
  function runOf(cell) {
    var box = boxOf(cell);
    while (
      box !== null &&
      box.childElementCount === 1 &&
      box.firstElementChild.tagName === "X-S"
    ) {
      box = box.firstElementChild;
    }
    return box;
  }

  // What the page shows at a position, shaped the way an op states a value.
  function valueAt(column, row) {
    var cell = cellAt(column, row);
    if (cell === null) {
      return null;
    }
    var text = cell.textContent.trim();
    if (text === "") {
      return { type: "empty" };
    }
    if (cell.classList.contains("odr-value-type-float")) {
      var number = toNumber(text);
      if (!isNaN(number)) {
        return { type: "number", number: number, text: text };
      }
    }
    return { type: "string", text: text };
  }

  // Shows @p value at a position, as a write leaves the cell, and reflows
  // the row around it.
  function showValue(column, row, value) {
    var cell = cellAt(column, row);
    if (cell === null) {
      return false;
    }
    lower();
    var run = runOf(cell);
    if (run === null) {
      return false;
    }
    run.textContent = value.type === "empty" ? "" : value.text;
    cell.classList.toggle("odr-value-type-float", value.type === "number");
    reflow(row);
    return true;
  }

  // What the script beside this one, and a host, ask of the sheet: positions
  // the way an op names them, and the pin. `spreadsheet-editing.md` decision 8.
  odr.sheet = {
    cellAt: cellAt,
    positionOf: positionOf,
    pinned: pinnedPosition,
    pin: pinAt,
    lower: lower,
    valueAt: valueAt,
    showValue: showValue,
    reflow: reflow,
  };

  table.addEventListener("mouseover", function (event) {
    var column = rulerColumn(event.target.closest("td,th"));
    if (column !== hovered) {
      hovered = column;
      paint();
    }
  });

  table.addEventListener("mouseleave", function () {
    hovered = -1;
    paint();
  });

  table.addEventListener("click", function (event) {
    // Selecting inside what is raised must not put the cell back.
    if (raisedContent !== null && raisedContent.contains(event.target)) {
      return;
    }

    var cell = event.target.closest("td,th");
    if (cell === null) {
      return;
    }

    // Clicking what is pinned clears it.
    if (cell === pinnedCell) {
      pin(-1, null, null);
      return;
    }

    if (cell.classList.contains("odr-sheet-column-header")) {
      pin(rulerColumn(cell), null, cell);
    } else if (cell.classList.contains("odr-sheet-row-header")) {
      pin(-1, cell.parentElement, cell);
    } else if (cell.classList.contains("odr-sheet-corner")) {
      pin(-1, null, null);
    } else {
      pin(rulerColumn(cell), cell.parentElement, cell);
    }
  });

  // The canvas around the sheet included.
  document.addEventListener("click", function (event) {
    if (event.target.closest(".odr-sheet") === null) {
      pin(-1, null, null);
    }
  });

  document.addEventListener("keydown", function (event) {
    if (event.key === "Escape") {
      pin(-1, null, null);
    }
  });

)js";

/// The rest of `spreadsheet_js`, which one literal cannot hold.
constexpr std::string_view spreadsheet_js_tail = R"js(
  var body = table.tBodies[0];
  var original = null;
  var sortedColumn = -1;
  var sortedDirection = 0;

  // Only the rendered text is in the markup, not the number behind it. The last
  // separator is the decimal one, which settles 1,234.56 against 1.234,56.
  function toNumber(text) {
    var cleaned = text.replace(/[^0-9,.eE+-]/g, "");
    if (cleaned.lastIndexOf(",") > cleaned.lastIndexOf(".")) {
      cleaned = cleaned.replace(/\./g, "").replace(",", ".");
    } else {
      cleaned = cleaned.replace(/,/g, "");
    }
    var value = parseFloat(cleaned);
    return isFinite(value) ? value : NaN;
  }

  // Numbers, then text, then blanks: no column is forced into one kind.
  var NUMBER = 0;
  var TEXT = 1;
  var BLANK = 2;

  function keyOf(row, index) {
    var cell = row.children[index];
    var text = cell === undefined ? "" : cell.textContent.trim();
    if (text === "") {
      return { rank: BLANK, value: 0 };
    }
    if (cell.classList.contains("odr-value-type-float")) {
      var value = toNumber(text);
      if (!isNaN(value)) {
        return { rank: NUMBER, value: value };
      }
    }
    return { rank: TEXT, value: text };
  }

  function reorder(rows) {
    var fragment = document.createDocumentFragment();
    for (var i = 0; i < rows.length; ++i) {
      fragment.appendChild(rows[i]);
    }
    body.appendChild(fragment);
  }

  function sortBy(index, direction) {
    if (original === null) {
      original = Array.prototype.slice.call(body.rows);
    }
    if (direction === 0) {
      reorder(original);
      return;
    }

    var rows = Array.prototype.slice.call(body.rows);
    var keys = new Map();
    for (var i = 0; i < rows.length; ++i) {
      keys.set(rows[i], keyOf(rows[i], index));
    }

    // A blank is an absent value, not the smallest one, so it stays last either
    // way round. The stable sort keeps the document's order for ties.
    rows.sort(function (a, b) {
      var x = keys.get(a);
      var y = keys.get(b);
      if (x.rank === BLANK || y.rank === BLANK) {
        return x.rank === y.rank ? 0 : x.rank === BLANK ? 1 : -1;
      }
      if (x.rank !== y.rank) {
        return (x.rank - y.rank) * direction;
      }
      var result =
        x.rank === NUMBER
          ? x.value - y.value
          : x.value.localeCompare(y.value, undefined, { numeric: true });
      return result * direction;
    });
    reorder(rows);
  }

  // A `rowspan` would reach into a row no longer beneath it and a `colspan`
  // breaks the column index, so a merged sheet gets no sort control.
  if (!merged) {
    var headers = table.tHead.rows[0].children;
    for (var column = 1; column < headers.length; ++column) {
      var control = document.createElement("span");
      control.className = "odr-sheet-sort";
      control.setAttribute("title", "sort by column " + headers[column].textContent);
      headers[column].appendChild(control);
    }

    table.addEventListener(
      "click",
      function (event) {
        var control = event.target.closest(".odr-sheet-sort");
        if (control === null) {
          return;
        }
        // The header itself pins the column; only this control sorts it.
        event.stopPropagation();

        var index = control.parentElement.cellIndex;
        var direction =
          index !== sortedColumn ? 1 : sortedDirection === 1 ? -1 : 0;

        sortBy(index, direction);

        control.classList.remove("odr-sheet-sort-asc", "odr-sheet-sort-desc");
        if (direction === 1) {
          control.classList.add("odr-sheet-sort-asc");
        } else if (direction === -1) {
          control.classList.add("odr-sheet-sort-desc");
        }
        if (sortedColumn !== index && sortedColumn >= 0) {
          headers[sortedColumn]
            .querySelector(".odr-sheet-sort")
            .classList.remove("odr-sheet-sort-asc", "odr-sheet-sort-desc");
        }

        sortedColumn = direction === 0 ? -1 : index;
        sortedDirection = direction;
      },
      true
    );
  }
})();
)js";

/// `odr.editing`: the mode, and the refusals the page reports to its host.
/// A sheet's editing is an overlay, so the markup states only what the page
/// cannot work out - the document's editability and a locked cell's reason.
constexpr std::string_view sheet_editing_js = R"js(
(function () {
  "use strict";

  var table = document.querySelector(".odr-sheet");
  if (table === null) {
    return;
  }

  var odr = (window.odr = window.odr || {});

  var sheet = Number(table.getAttribute("data-odr-sheet") || 0);
  var editable = table.getAttribute("data-odr-editable") === "true";
  var editing = false;
  var lastRefusal = null;

  // One space with `odr.onError`'s codes, appended and never renumbered - 1 is
  // `errorIllegalEditNewLine`. The host maps the code to its own wording; the
  // message is for a developer who wires nothing.
  var refusals = {
    formula: { code: 2, message: "cell holds a formula" },
    rich: { code: 3, message: "cell holds more than one plain run" },
    shapes: { code: 4, message: "cell holds a drawing" },
    readOnly: { code: 5, message: "document cannot be edited" },
    formulaInput: { code: 6, message: "typing a formula is not supported" },
  };

  odr.onEditRefused = function (event) {
    console.warn("edit refused " + event.code + ": " + event.message);
  };
  odr.onEditModeChange = function (event) {
    console.log("editing " + (event.editing ? "on" : "off"));
  };
  odr.onEditChange = function () {};

  function fire(name, event) {
    if (typeof odr[name] === "function") {
      odr[name](event);
    }
  }

  var outlined = null;
  var outlinedTimer = 0;

  /// The outline a refusal paints, so a host wiring nothing is not silent.
  function outline(cell) {
    if (outlined !== null) {
      outlined.classList.remove("odr-sheet-refused");
    }
    window.clearTimeout(outlinedTimer);
    outlined = cell;
    if (cell === null) {
      return;
    }
    cell.classList.add("odr-sheet-refused");
    outlinedTimer = window.setTimeout(function () {
      cell.classList.remove("odr-sheet-refused");
      outlined = null;
    }, 700);
  }

  /// Four taps on a locked cell are one snackbar: the same refusal within two
  /// seconds of the last is the page's to drop. The outline answers each.
  function refuse(reason, column, row) {
    var refusal = refusals[reason] || refusals.readOnly;
    var key = reason + ":" + column + ":" + row;
    var now = Date.now();
    outline(odr.sheet.cellAt(column, row));
    if (lastRefusal && lastRefusal.key === key && now - lastRefusal.at < 2000) {
      return;
    }
    lastRefusal = { key: key, at: now };
    fire("onEditRefused", {
      sheet: sheet,
      column: column,
      row: row,
      reason: reason,
      code: refusal.code,
      message: refusal.message,
    });
  }

  function modeChange(reason) {
    fire("onEditModeChange", {
      editing: editing,
      editable: editable,
      reason: reason || null,
      code: reason ? refusals[reason].code : 0,
      message: reason ? refusals[reason].message : "",
    });
  }

  odr.editing = {
    /// Answers whether the mode is on. A document that cannot be edited
    /// refuses and says why, so a host can grey its button before a click.
    enable: function () {
      if (!editable) {
        modeChange("readOnly");
        return false;
      }
      if (!editing) {
        editing = true;
        table.classList.add("odr-editing");
        modeChange(null);
      }
      return true;
    },
    disable: function () {
      if (editing) {
        editing = false;
        close();
        table.classList.remove("odr-editing");
        modeChange(null);
      }
    },
    isEnabled: function () {
      return editing;
    },
    /// Whether `enable` would succeed.
    isEditable: function () {
      return editable;
    },
    /// The lock on the cell at (@p column, @p row), or null where it has none.
    lockAt: function (column, row) {
      var cell = odr.sheet.cellAt(column, row);
      return cell === null ? null : cell.getAttribute("data-odr-lock");
    },
  };

  /// Whether the cell at (@p column, @p row) refuses a write, which is also
  /// what tells the host.
  odr.editing.refuseAt = function (column, row) {
    if (!editable) {
      refuse("readOnly", column, row);
      return true;
    }
    var lock = odr.editing.lockAt(column, row);
    if (lock !== null) {
      refuse(lock, column, row);
      return true;
    }
    return false;
  };

  var overlay = null;
  var editingAt = null;
  var history = [];

  var NUMBER = /^[+-]?([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?$/;

  /// The type follows the string the user typed: a number where the grammar
  /// says so, a string otherwise, and a leading `'` forces one.
  function parse(text) {
    var quoted = text.charAt(0) === "'";
    var content = quoted ? text.slice(1) : text;
    if (content === "") {
      return { type: "empty" };
    }
    if (!quoted && NUMBER.test(content)) {
      return { type: "number", number: Number(content), text: content };
    }
    return { type: "string", text: content };
  }

  function same(one, other) {
    return (
      one.type === other.type &&
      (one.type === "empty" || one.text === other.text)
    );
  }

  /// Writes @p value at a position: the cell shows it, and the op joins the
  /// log beside the value it replaced.
  function write(column, row, value) {
    var before = odr.sheet.valueAt(column, row);
    if (before === null || same(before, value)) {
      return false;
    }
    if (!odr.sheet.showValue(column, row, value)) {
      return false;
    }
    history.push({
      op: {
        op: "setCell",
        sheet: sheet,
        column: column,
        row: row,
        value: value,
      },
      before: before,
    });
    return true;
  }

  // Offsets, not rects: blink scales a rect by the body zoom `viewport_js`
  // applies, and the overlay is laid out under that zoom.
  function place(cell) {
    var left = 0;
    var top = 0;
    for (var node = cell; node !== null; node = node.offsetParent) {
      left += node.offsetLeft;
      top += node.offsetTop;
    }
    var style = getComputedStyle(cell);
    overlay.style.left = left + "px";
    overlay.style.top = top + "px";
    overlay.style.width = cell.offsetWidth + "px";
    overlay.style.height = cell.offsetHeight + "px";
    overlay.style.textAlign = style.textAlign;
    overlay.style.color = style.color;
    overlay.style.fontFamily = style.fontFamily;
    overlay.style.fontSize = style.fontSize;
    overlay.style.fontStyle = style.fontStyle;
    overlay.style.fontWeight = style.fontWeight;
  }

  /// Opens the editor over a cell, holding @p typed or the cell's own string.
  /// The raise is put back down: the overlay shows what it would have.
  function edit(column, row, typed) {
    finish();
    if (!editing || odr.editing.refuseAt(column, row)) {
      return false;
    }
    var cell = odr.sheet.cellAt(column, row);
    if (cell === null) {
      return false;
    }
    odr.sheet.pin({ column: column, row: row });
    odr.sheet.lower();

    var value = odr.sheet.valueAt(column, row);
    editingAt = { column: column, row: row };
    overlay = document.createElement("input");
    overlay.type = "text";
    overlay.className = "odr-sheet-editor";
    overlay.value =
      typed !== null ? typed : value.type === "empty" ? "" : value.text;
    place(cell);
    document.body.appendChild(overlay);
    overlay.addEventListener("keydown", overlayKey);
    overlay.addEventListener("blur", finish);
    overlay.focus();
    if (typed === null) {
      overlay.select();
    }
    return true;
  }

  function close() {
    var input = overlay;
    overlay = null;
    editingAt = null;
    if (input !== null) {
      input.remove();
    }
  }

  /// Ends an open edit: what it holds is committed, and a refused formula is
  /// dropped rather than left in an overlay nothing focuses again.
  function finish() {
    if (!commit(0, 0)) {
      close();
    }
  }

  /// Commits what is typed and moves the pin by (@p columns, @p rows). A
  /// formula is refused rather than written, and leaves the editor open.
  function commit(columns, rows) {
    if (overlay === null) {
      return false;
    }
    var text = overlay.value;
    var at = editingAt;
    if (text.charAt(0) === "=") {
      refuse("formulaInput", at.column, at.row);
      return false;
    }
    close();
    write(at.column, at.row, parse(text));
    if (!odr.sheet.pin({ column: at.column + columns, row: at.row + rows })) {
      odr.sheet.pin({ column: at.column, row: at.row });
    }
    return true;
  }

  function overlayKey(event) {
    // Typing is the overlay's, not the sheet's underneath it.
    event.stopPropagation();
    if (event.key === "Escape") {
      close();
    } else if (event.key === "Enter") {
      commit(0, event.shiftKey ? -1 : 1);
    } else if (event.key === "Tab") {
      commit(event.shiftKey ? -1 : 1, 0);
    } else {
      return;
    }
    event.preventDefault();
  }

  var arrows = {
    ArrowUp: [0, -1],
    ArrowDown: [0, 1],
    ArrowLeft: [-1, 0],
    ArrowRight: [1, 0],
  };

  /// What a pinned cell does with a key when no editor is open. Captured, so
  /// the keys taken here never reach the pin and the sort beneath.
  function pinnedKey(event) {
    if (
      !editing ||
      overlay !== null ||
      event.ctrlKey ||
      event.metaKey ||
      event.altKey
    ) {
      return;
    }
    var target = event.target;
    if (
      target &&
      (target.isContentEditable ||
        /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName))
    ) {
      return;
    }
    var at = odr.sheet.pinned();
    if (at === null || at.column === null || at.row === null) {
      return;
    }

    var step =
      arrows[event.key] ||
      (event.key === "Tab" ? [event.shiftKey ? -1 : 1, 0] : null);
    if (step !== null) {
      odr.sheet.pin({ column: at.column + step[0], row: at.row + step[1] });
    } else if (event.key === "Enter" || event.key === "F2") {
      edit(at.column, at.row, null);
    } else if (event.key === "Delete" || event.key === "Backspace") {
      if (!odr.editing.refuseAt(at.column, at.row)) {
        write(at.column, at.row, { type: "empty" });
      }
    } else if (event.key.length === 1) {
      edit(at.column, at.row, event.key);
    } else {
      return;
    }
    event.stopPropagation();
    event.preventDefault();
  }

  document.addEventListener("keydown", pinnedKey, true);

  window.addEventListener("resize", function () {
    if (overlay !== null) {
      place(odr.sheet.cellAt(editingAt.column, editingAt.row));
    }
  });

  function targetPosition(event) {
    return odr.sheet.positionOf(event.target.closest("td"));
  }

  table.addEventListener("dblclick", function (event) {
    var at = editing ? targetPosition(event) : null;
    if (at !== null) {
      edit(at.column, at.row, null);
    }
  });

  // A locked cell says so on the click, not on the double click.
  table.addEventListener("click", function (event) {
    var at = editing && overlay === null ? targetPosition(event) : null;
    if (at !== null && odr.editing.lockAt(at.column, at.row) !== null) {
      odr.editing.refuseAt(at.column, at.row);
    }
  });

  /// Opens the editor over a cell, as a double click does.
  odr.editing.editAt = function (column, row) {
    return edit(column, row, null);
  };

  /// What a host hands to `Document::edit` before saving, coalesced per
  /// position.
  odr.editing.getOperations = function () {
    var byPosition = new Map();
    for (var i = 0; i < history.length; ++i) {
      var op = history[i].op;
      byPosition.set(op.sheet + ":" + op.column + ":" + op.row, op);
    }
    return JSON.stringify({
      version: 1,
      ops: Array.from(byPosition.values()),
    });
  };
})();
)js";

/// Every input is applied to the line `<div>`s by hand, so the line numbers
/// stay in step and undo/redo replay changes instead of the browser's history.
constexpr std::string_view text_js = R"js(
(function () {
  "use strict";

  function TextEditor(textNr, textBody) {
    this.textNr = textNr;
    this.textBody = textBody;
    this.past = [];
    this.future = [];

    var self = this;

    new ResizeObserver(function () {
      self.updateLineNumberHeight();
    }).observe(this.textBody);

    this.textBody.addEventListener("input", function () {
      var nrCount = self.textNr.querySelectorAll("div").length;
      var lineCount = self.textBody.querySelectorAll("div").length;
      for (var i = nrCount + 1; i <= lineCount; ++i) {
        var nrCell = document.createElement("div");
        nrCell.textContent = String(i);
        self.textNr.appendChild(nrCell);
      }
      for (var j = nrCount; j > lineCount; --j) {
        self.textNr.removeChild(self.textNr.lastChild);
      }
      self.updateLineNumberHeight();
    });

    this.textBody.addEventListener("beforeinput", function (event) {
      event.preventDefault();

      if (event.inputType === "historyUndo") {
        self.undo();
      } else if (event.inputType === "historyRedo") {
        self.redo();
      } else if (event.inputType === "insertText") {
        self.insertTextAction(event.data);
      } else if (event.inputType === "insertParagraph") {
        self.insertTextAction("\n");
      } else if (event.inputType === "deleteContentBackward") {
        self.removeTextAction("backward");
      } else if (event.inputType === "deleteContentForward") {
        self.removeTextAction("forward");
      }
    });

    this.textBody.addEventListener("paste", function (event) {
      event.preventDefault();
      self.insertTextAction(event.clipboardData.getData("text/plain"));
    });

    this.textBody.addEventListener("drop", function (event) {
      event.preventDefault();
    });

    this.textBody.addEventListener("dragover", function (event) {
      event.preventDefault();
    });
  }

  // The measured height is fractional; `offsetHeight` would round it per line
  // and the numbers would walk away from the lines they belong to.
  //
  // Every height is read before any is written: interleaving them makes each
  // read force the layout the write before it invalidated, one per line.
  TextEditor.prototype.updateLineNumberHeight = function () {
    var nrCells = this.textNr.querySelectorAll("div");
    var textCells = this.textBody.querySelectorAll("div");
    var count = Math.min(textCells.length, nrCells.length);
    var heights = new Array(count);
    for (var i = 0; i < count; ++i) {
      heights[i] = textCells[i].getBoundingClientRect().height;
    }
    for (var j = 0; j < count; ++j) {
      nrCells[j].style.height = heights[j] + "px";
    }
  };

  // Lines are the element children: formatted output puts a whitespace text
  // node between them, and counting or indexing those as lines is off by as
  // much as a factor of two. The line is the ancestor the body owns and the
  // offset is measured from its start: a search `<mark>` may sit in between.
  TextEditor.prototype.getPosition = function (container, offset) {
    var line = container;
    while (line !== null && line.parentNode !== this.textBody) {
      line = line.parentNode;
    }
    if (line === null) {
      return { line: -1, offset: offset };
    }
    var range = document.createRange();
    range.selectNodeContents(line);
    range.setEnd(container, offset);
    return {
      line: Array.prototype.indexOf.call(this.textBody.children, line),
      offset: range.toString().length,
    };
  };

  TextEditor.prototype.getLine = function (lineNr) {
    return this.textBody.children[lineNr];
  };

  TextEditor.prototype.getLineText = function (line) {
    return line.textContent;
  };

  TextEditor.prototype.setLineText = function (line, text) {
    line.textContent = text;
    if (text === "") {
      line.appendChild(document.createElement("br"));
    }
  };

  // Counts a line break as one character.
  TextEditor.prototype.movePosition = function (position, delta) {
    var remainingDelta = Math.abs(delta);
    var sign = delta >= 0 ? 1 : -1;

    var lineNr = position.line;
    var offset = position.offset;
    var line = this.getLine(lineNr);
    var lineLength = this.getLineText(line).length;

    while (true) {
      var remaining = sign > 0 ? lineLength - offset : offset;
      var step = Math.min(remaining, remainingDelta);
      offset += sign * step;
      remainingDelta -= step;
      if (remainingDelta === 0) {
        break;
      }

      line = sign > 0 ? line.nextElementSibling : line.previousElementSibling;
      if (line === null) {
        break;
      }
      lineLength = this.getLineText(line).length;
      lineNr += sign;
      offset = sign > 0 ? 0 : lineLength;
      remainingDelta -= 1;
    }

    return { line: lineNr, offset: offset };
  };

  TextEditor.prototype.getText = function (from, to) {
    var result = "";
    for (var lineNr = from.line; lineNr <= to.line; ++lineNr) {
      if (lineNr > from.line) {
        result += "\n";
      }
      var lineText = this.getLineText(this.getLine(lineNr));
      if (from.line === to.line) {
        result += lineText.slice(from.offset, to.offset);
      } else if (lineNr === from.line) {
        result += lineText.slice(from.offset);
      } else if (lineNr === to.line) {
        result += lineText.slice(0, to.offset);
      } else {
        result += lineText;
      }
    }
    return result;
  };

  TextEditor.prototype.insertText = function (position, text) {
    var textLines = text.split("\n");

    var line = this.getLine(position.line);
    var originalText = this.getLineText(line);

    if (textLines.length === 1) {
      this.setLineText(
        line,
        originalText.slice(0, position.offset) +
          textLines[0] +
          originalText.slice(position.offset)
      );
      return {
        line: position.line,
        offset: position.offset + textLines[0].length,
      };
    }

    for (var i = 0; i < textLines.length; ++i) {
      if (i > 0) {
        this.textBody.insertBefore(
          document.createElement("div"),
          line.nextElementSibling
        );
        line = line.nextElementSibling;

        this.textNr.appendChild(document.createElement("div"));
        // the line is already in, so the count is the number the cell gets
        this.textNr.lastChild.textContent = String(this.textBody.children.length);
      }

      if (i === 0) {
        this.setLineText(line, originalText.slice(0, position.offset) + textLines[i]);
      } else if (i === textLines.length - 1) {
        this.setLineText(line, textLines[i] + originalText.slice(position.offset));
      } else {
        this.setLineText(line, textLines[i]);
      }
    }

    return {
      line: position.line + textLines.length - 1,
      offset: textLines[textLines.length - 1].length,
    };
  };

  TextEditor.prototype.removeText = function (from, to) {
    var firstLine = this.getLine(from.line);
    var lastLine = this.getLine(to.line);

    this.setLineText(
      firstLine,
      this.getLineText(firstLine).slice(0, from.offset) +
        this.getLineText(lastLine).slice(to.offset)
    );

    for (var lineNr = from.line + 1; lineNr <= to.line; ++lineNr) {
      this.textBody.removeChild(firstLine.nextElementSibling);
      this.textNr.removeChild(this.textNr.lastChild);
    }
  };

  TextEditor.prototype.placeCursorAt = function (position) {
    var line = this.getLine(position.line);
    var range = document.createRange();
    range.setStart(line.firstChild, position.offset);
    range.setEnd(line.firstChild, position.offset);
    range.collapse(true);

    var selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
  };

  TextEditor.prototype.doChange = function (change) {
    if (change.type === "insertText") {
      this.insertText(change.position, change.text);
    } else if (change.type === "removeText") {
      this.removeText(
        change.position,
        this.movePosition(change.position, change.text.length)
      );
    }
  };

  TextEditor.prototype.invertChange = function (change) {
    return {
      type: change.type === "insertText" ? "removeText" : "insertText",
      text: change.text,
      position: change.position,
    };
  };

  TextEditor.prototype.pushChange = function (change) {
    this.past.push(change);
    this.future = [];
  };

  TextEditor.prototype.undo = function () {
    if (this.past.length === 0) {
      return;
    }
    var change = this.past.pop();
    this.future.push(change);
    this.doChange(this.invertChange(change));
  };

  TextEditor.prototype.redo = function () {
    if (this.future.length === 0) {
      return;
    }
    var change = this.future.pop();
    this.past.push(change);
    this.doChange(change);
  };

  TextEditor.prototype.insertTextAction = function (text) {
    var selection = window.getSelection();
    if (selection.rangeCount !== 1) {
      console.log("Multiple selection ranges, not supported");
      return;
    }
    var range = selection.getRangeAt(0);
    var position = this.getPosition(range.startContainer, range.startOffset);

    if (
      range.startContainer !== range.endContainer ||
      range.startOffset !== range.endOffset
    ) {
      this.removeTextAction("backward");
    }

    var newPosition = this.insertText(position, text);
    this.pushChange({ type: "insertText", text: text, position: position });
    this.placeCursorAt(newPosition);
  };

  TextEditor.prototype.removeTextAction = function (mode) {
    var selection = window.getSelection();
    if (selection.rangeCount !== 1) {
      console.log("Multiple selection ranges, not supported");
      return;
    }
    var range = selection.getRangeAt(0);
    var startPosition = this.getPosition(range.startContainer, range.startOffset);
    var endPosition = this.getPosition(range.endContainer, range.endOffset);
    var isSelected =
      range.startContainer !== range.endContainer ||
      range.startOffset !== range.endOffset;

    var from = isSelected
      ? startPosition
      : mode === "forward"
        ? startPosition
        : this.movePosition(startPosition, -1);
    var to = isSelected
      ? endPosition
      : mode === "forward"
        ? this.movePosition(endPosition, 1)
        : endPosition;

    if (from.line === to.line && from.offset === to.offset) {
      console.log("No text to remove");
      return;
    }

    var removedText = this.getText(from, to);
    this.removeText(from, to);
    this.pushChange({
      type: "removeText",
      text: removedText,
      position: from,
    });
    this.placeCursorAt(from);
  };

  var textNr = document.querySelector(".odr-text-nr");
  var textBody = document.querySelector(".odr-text-body");
  if (textNr && textBody) {
    new TextEditor(textNr, textBody);
  }
})();
)js";

/// One of the renderer's own stylesheets or scripts — "shipped" in the sense
/// @ref odr::HtmlConfig::embed_shipped_resources means.
struct Asset {
  HtmlResourceType type;
  std::string_view mime_type;
  std::string_view name;
  std::string_view content;
  std::string_view content_tail{}; ///< written straight after @ref content
};

/// msvc caps a string literal at 16380 bytes, and a windows checkout spends one
/// more per line, so a script outgrowing that is split over two.
consteval bool fits_a_literal(const std::string_view content) {
  return content.size() + std::ranges::count(content, '\n') <= 16380;
}

static_assert(fits_a_literal(viewport_js));
static_assert(fits_a_literal(search_js));
static_assert(fits_a_literal(spreadsheet_js));
static_assert(fits_a_literal(spreadsheet_js_tail));
static_assert(fits_a_literal(sheet_editing_js));
static_assert(fits_a_literal(text_js));
static_assert(fits_a_literal(pdf_annotation_js));
static_assert(fits_a_literal(pdf_annotation_js_tail));

constexpr Asset document_css_asset{HtmlResourceType::css, "text/css",
                                   "document.css", document_css};
constexpr Asset document_dark_css_asset{HtmlResourceType::css, "text/css",
                                        "document-dark.css", document_dark_css};
constexpr Asset spreadsheet_css_asset{HtmlResourceType::css, "text/css",
                                      "spreadsheet.css", spreadsheet_css};
constexpr Asset spreadsheet_dark_css_asset{HtmlResourceType::css, "text/css",
                                           "spreadsheet-dark.css",
                                           spreadsheet_dark_css};
constexpr Asset text_css_asset{HtmlResourceType::css, "text/css", "text.css",
                               text_css};
constexpr Asset text_dark_css_asset{HtmlResourceType::css, "text/css",
                                    "text-dark.css", text_dark_css};
constexpr Asset xml_css_asset{HtmlResourceType::css, "text/css", "xml.css",
                              xml_css};
constexpr Asset xml_dark_css_asset{HtmlResourceType::css, "text/css",
                                   "xml-dark.css", xml_dark_css};
constexpr Asset filesystem_css_asset{HtmlResourceType::css, "text/css",
                                     "filesystem.css", filesystem_css};
constexpr Asset filesystem_dark_css_asset{HtmlResourceType::css, "text/css",
                                          "filesystem-dark.css",
                                          filesystem_dark_css};
constexpr Asset media_css_asset{HtmlResourceType::css, "text/css", "media.css",
                                media_css};
constexpr Asset search_css_asset{HtmlResourceType::css, "text/css",
                                 "search.css", search_css};
constexpr Asset search_dark_css_asset{HtmlResourceType::css, "text/css",
                                      "search-dark.css", search_dark_css};
constexpr Asset document_js_asset{HtmlResourceType::js, "text/javascript",
                                  "document.js", document_js};
constexpr Asset search_js_asset{HtmlResourceType::js, "text/javascript",
                                "search.js", search_js};
constexpr Asset spreadsheet_js_asset{HtmlResourceType::js, "text/javascript",
                                     "spreadsheet.js", spreadsheet_js,
                                     spreadsheet_js_tail};
constexpr Asset sheet_editing_js_asset{HtmlResourceType::js, "text/javascript",
                                       "sheet-editing.js", sheet_editing_js};
constexpr Asset text_js_asset{HtmlResourceType::js, "text/javascript",
                              "text.js", text_js};
constexpr Asset viewport_js_asset{HtmlResourceType::js, "text/javascript",
                                  "viewport.js", viewport_js};
constexpr Asset pdf_annotation_css_asset{HtmlResourceType::css, "text/css",
                                         "pdf-annotation.css",
                                         pdf_annotation_css};
constexpr Asset pdf_annotation_js_asset{HtmlResourceType::js, "text/javascript",
                                        "pdf-annotation.js", pdf_annotation_js,
                                        pdf_annotation_js_tail};

/// Appends @p asset to @p resources; `nullopt` to embed it.
HtmlResourceLocation locate(const Asset &asset, const HtmlConfig &config,
                            HtmlResources &resources) {
  const odr::HtmlResource resource = HtmlResource::create(
      asset.type, std::string(asset.mime_type), std::string(asset.name),
      std::string(asset.name),
      odr::File::from_memory(std::string(asset.content) +
                             std::string(asset.content_tail)),
      true, false, true);
  HtmlResourceLocation location = config.resource_locator(resource, config);
  resources.emplace_back(resource, location);
  return location;
}

HtmlResources locate_all(const std::span<const Asset> assets,
                         const HtmlConfig &config) {
  HtmlResources resources;
  for (const Asset &asset : assets) {
    locate(asset, config, resources);
  }
  return resources;
}

/// Adds the dark sheets where the config asks for them.
HtmlResources locate_all(const std::span<const Asset> assets,
                         const std::span<const Asset> dark,
                         const HtmlConfig &config) {
  HtmlResources resources = locate_all(assets, config);
  if (writes_dark_style(config)) {
    for (const Asset &asset : dark) {
      locate(asset, config, resources);
    }
  }
  return resources;
}

/// @p media, when given, gates the stylesheet on that media query.
void write_style(const Asset &asset, const WritingState &state,
                 const std::string_view media = {}) {
  if (const HtmlResourceLocation location =
          locate(asset, state.config(), state.resources());
      location.has_value()) {
    state.out().write_header_style(xml::escape_attribute(*location), media);
    return;
  }

  state.out().write_header_style_begin(media);
  state.out().out() << asset.content << asset.content_tail;
  state.out().write_header_style_end();
}

void write_dark_style(const Asset &asset, const WritingState &state) {
  if (writes_dark_style(state.config())) {
    write_style(asset, state, dark_style_media(state.config()));
  }
}

void write_script(const Asset &asset, const WritingState &state) {
  if (const HtmlResourceLocation location =
          locate(asset, state.config(), state.resources());
      location.has_value()) {
    state.out().write_script(xml::escape_attribute(*location));
    return;
  }

  state.out().write_script_begin();
  state.out().out() << asset.content << asset.content_tail;
  state.out().write_script_end();
}

} // namespace

} // namespace odr::internal::html

namespace odr::internal {

bool html::writes_dark_style(const HtmlConfig &config) {
  return config.color_scheme != HtmlColorScheme::light;
}

std::string_view html::dark_style_media(const HtmlConfig &config) {
  return config.color_scheme == HtmlColorScheme::system
             ? "(prefers-color-scheme: dark)"
             : "";
}

void html::write_document_style(const WritingState &state) {
  write_style(document_css_asset, state);
}

void html::write_spreadsheet_style(const WritingState &state) {
  write_style(spreadsheet_css_asset, state);
}

void html::write_document_dark_style(const WritingState &state) {
  write_dark_style(document_dark_css_asset, state);
}

void html::write_spreadsheet_dark_style(const WritingState &state) {
  write_dark_style(spreadsheet_dark_css_asset, state);
}

void html::write_text_style(const WritingState &state) {
  write_style(text_css_asset, state);
}

void html::write_text_dark_style(const WritingState &state) {
  write_dark_style(text_dark_css_asset, state);
}

void html::write_xml_style(const WritingState &state) {
  write_style(xml_css_asset, state);
}

void html::write_xml_dark_style(const WritingState &state) {
  write_dark_style(xml_dark_css_asset, state);
}

void html::write_filesystem_style(const WritingState &state) {
  write_style(filesystem_css_asset, state);
}

void html::write_filesystem_dark_style(const WritingState &state) {
  write_dark_style(filesystem_dark_css_asset, state);
}

void html::write_media_style(const WritingState &state) {
  write_style(media_css_asset, state);
}

void html::write_search_style(const WritingState &state) {
  write_style(search_css_asset, state);
}

void html::write_search_dark_style(const WritingState &state) {
  write_dark_style(search_dark_css_asset, state);
}

void html::write_document_script(const WritingState &state) {
  write_script(document_js_asset, state);
}

void html::write_search_script(const WritingState &state) {
  write_script(search_js_asset, state);
}

void html::write_spreadsheet_script(const WritingState &state) {
  write_script(spreadsheet_js_asset, state);
  write_script(sheet_editing_js_asset, state);
}

void html::write_text_script(const WritingState &state) {
  write_script(text_js_asset, state);
}

void html::write_pdf_annotation_style(const WritingState &state) {
  write_style(pdf_annotation_css_asset, state);
}

void html::write_pdf_annotation_script(const WritingState &state) {
  write_script(pdf_annotation_js_asset, state);
}

void html::write_viewport_script(const WritingState &state) {
  write_script(viewport_js_asset, state);
}

HtmlResources html::locate_text_resources(const HtmlConfig &config) {
  static constexpr std::array assets{text_css_asset, search_css_asset,
                                     search_js_asset, text_js_asset,
                                     viewport_js_asset};
  static constexpr std::array dark{text_dark_css_asset, search_dark_css_asset};
  return locate_all(assets, dark, config);
}

HtmlResources html::locate_xml_resources(const HtmlConfig &config) {
  static constexpr std::array assets{xml_css_asset, search_css_asset,
                                     search_js_asset, viewport_js_asset};
  static constexpr std::array dark{xml_dark_css_asset, search_dark_css_asset};
  return locate_all(assets, dark, config);
}

HtmlResources html::locate_search_resources(const HtmlConfig &config) {
  static constexpr std::array assets{search_css_asset, search_js_asset};
  return locate_all(assets, config);
}

HtmlResources html::locate_viewport_resources(const HtmlConfig &config) {
  static constexpr std::array assets{viewport_js_asset};
  return locate_all(assets, config);
}

HtmlResources html::locate_pdf_annotation_resources(const HtmlConfig &config) {
  static constexpr std::array assets{pdf_annotation_css_asset,
                                     pdf_annotation_js_asset};
  return locate_all(assets, config);
}

HtmlResources html::locate_media_resources(const HtmlConfig &config) {
  static constexpr std::array assets{media_css_asset};
  return locate_all(assets, config);
}

} // namespace odr::internal
