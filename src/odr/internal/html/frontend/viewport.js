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
