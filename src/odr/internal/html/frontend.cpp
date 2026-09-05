#include <odr/internal/html/frontend.hpp>

#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/xml/xml_util.hpp>

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
--odr-sheet-font:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Helvetica Neue",Arial,sans-serif;
}
/* A sheet is not a page: past the last row and column is canvas. */
body{margin:0;background:var(--odr-sheet-canvas)}
.odr-sheet{background:#fff;border-collapse:collapse;table-layout:fixed}
/* The sheet's own cells, not a table the document itself drew inside one. */
.odr-sheet>tbody>tr>td{vertical-align:bottom;height:inherit;padding:1px 6px}
.odr-sheet>tbody>tr>td>x-p{height:inherit}
/* The font anything in a cell falls back to, a shape's text included, where the
   file names none of its own. */
.odr-sheet>tbody>tr>td x-p{font-family:var(--odr-sheet-font);font-size:10pt}
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
/* The header's `position:sticky` already makes it a containing block. */
.odr-sheet-sort{position:absolute;top:1px;right:1px;bottom:1px;width:17px;display:flex;align-items:center;justify-content:center;border-radius:2px;opacity:0;cursor:pointer}
.odr-sheet-column-header:hover .odr-sheet-sort,.odr-sheet-sort-asc,.odr-sheet-sort-desc{opacity:1}
.odr-sheet-sort:hover{background:var(--odr-sheet-wash-ruler)}
.odr-sheet-sort::after{content:"\25BE";font-size:15px;line-height:1}
.odr-sheet-sort-asc::after{content:"\25B4"}
/* Paper cannot scroll, so a sheet wider than the page is cut off; only
   `!important` beats the inline column width. The ruler collapses rather than
   `display:none`, which would take the cells out of their rows and shift every
   column left. */
@media print{
.odr-sheet thead{display:none}
.odr-sheet-gutter{visibility:collapse;width:0}
.odr-sheet-row-header{visibility:collapse;padding:0;box-shadow:none}
.odr-sheet{max-width:100%}
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
    var result = { modifiedText: {} };
    for (var path in modified) {
      if (Object.prototype.hasOwnProperty.call(modified, path)) {
        result.modifiedText[path] = modified[path].innerText;
      }
    }
    return JSON.stringify(result);
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

  function columnOf(cell) {
    return cell !== null && !merged ? cell.cellIndex : -1;
  }

  function pin(column, row, cell) {
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
    }
    paint();
  }

  table.addEventListener("mouseover", function (event) {
    var column = columnOf(event.target.closest("td,th"));
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
      pin(columnOf(cell), null, cell);
    } else if (cell.classList.contains("odr-sheet-row-header")) {
      pin(-1, cell.parentElement, cell);
    } else if (cell.classList.contains("odr-sheet-corner")) {
      pin(-1, null, null);
    } else {
      pin(columnOf(cell), cell.parentElement, cell);
    }
  });

  document.addEventListener("keydown", function (event) {
    if (event.key === "Escape") {
      pin(-1, null, null);
    }
  });

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
};

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
                                     "spreadsheet.js", spreadsheet_js};
constexpr Asset text_js_asset{HtmlResourceType::js, "text/javascript",
                              "text.js", text_js};
constexpr Asset viewport_js_asset{HtmlResourceType::js, "text/javascript",
                                  "viewport.js", viewport_js};

/// Appends @p asset to @p resources; `nullopt` to embed it.
HtmlResourceLocation locate(const Asset &asset, const HtmlConfig &config,
                            HtmlResources &resources) {
  const odr::HtmlResource resource = HtmlResource::create(
      asset.type, std::string(asset.mime_type), std::string(asset.name),
      std::string(asset.name),
      odr::File::from_memory(std::string(asset.content)), true, false, true);
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
  state.out().out() << asset.content;
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
  state.out().out() << asset.content;
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
}

void html::write_text_script(const WritingState &state) {
  write_script(text_js_asset, state);
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

HtmlResources html::locate_media_resources(const HtmlConfig &config) {
  static constexpr std::array assets{media_css_asset};
  return locate_all(assets, config);
}

} // namespace odr::internal
