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
  //
  // In a document of its own: a zoom on the body and an adjustment on the root
  // are inherited, so in the page they would restyle and lay out all of it,
  // twice.
  function textAdjustHolds() {
    var frame = document.createElement("iframe");
    frame.style.cssText =
      "position:absolute;top:0;left:0;width:400px;height:0;border:0;" +
      "visibility:hidden";
    body.appendChild(frame);
    var probe = frame.contentDocument;
    if (probe === null || probe.body === null) {
      body.removeChild(frame);
      return false;
    }
    probe.body.style.margin = "0";
    var ruler = probe.createElement("div");
    ruler.style.cssText =
      "position:absolute;top:0;left:0;width:400px;height:0;overflow:hidden";
    var run = probe.createElement("span");
    run.style.cssText = "font:100px/1 monospace;white-space:pre";
    run.textContent = "MMMMMMMMMM";
    ruler.appendChild(run);
    probe.body.appendChild(ruler);

    function ratio() {
      var length = ruler.getBoundingClientRect().width;
      return length ? run.getBoundingClientRect().width / length : 0;
    }

    var unzoomed = ratio();
    probe.body.style.zoom = "0.5";
    probe.documentElement.style.setProperty("-webkit-text-size-adjust", "50%");
    probe.documentElement.style.setProperty("text-size-adjust", "50%");
    var zoomed = ratio();
    body.removeChild(frame);

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

  // The browser lays out a chunk within a screen of the viewport, and not one
  // further away, so it holds the layout of a few chunks at a time. A chunk
  // that becomes near says so, because what is in it has no geometry before.
  // A held chunk stays near: something drawn in it reaches past it.
  function setNear(chunk, isNear) {
    if (isNear) {
      load(chunk);
    }
    if (chunk.classList.contains("odr-chunk-near") === isNear) {
      return;
    }
    chunk.classList.toggle("odr-chunk-near", isNear);
    if (isNear) {
      chunk.dispatchEvent(new CustomEvent("odr-chunk-near", { bubbles: true }));
    }
  }

  // A chunk that names its source has its content in a script of its own,
  // which puts it in through `odr.chunks.fill` (`lazy-loading.md` decision 2).
  // A script loads over HTTP and from a file alike.
  function load(chunk) {
    var source = chunk.getAttribute("data-odr-chunk-src");
    if (source === null || chunk.hasAttribute("data-odr-chunk-loading")) {
      return;
    }
    chunk.setAttribute("data-odr-chunk-loading", "");
    var script = document.createElement("script");
    script.src = source;
    // what it put in stays; its source need not
    script.onload = function () {
      script.remove();
    };
    script.onerror = function () {
      chunk.removeAttribute("data-odr-chunk-loading");
      script.remove();
    };
    document.head.appendChild(script);
  }

  // The text of the chunks that have not loaded, by chunk key, which the
  // scripts of the search index state (`lazy-loading.md` decision 4), folded
  // where `search.js` is there to fold it. A script that failed has no text,
  // and is not asked for again.
  var texts = {};
  var indexes = {};

  function loadIndex(source, then) {
    var state = indexes[source];
    if (state === undefined) {
      state = indexes[source] = { done: false, waiting: [] };
      var script = document.createElement("script");
      script.src = source;
      script.onload = script.onerror = function () {
        script.remove();
        state.done = true;
        state.waiting.forEach(function (waiting) {
          waiting();
        });
        state.waiting = [];
      };
      document.head.appendChild(script);
    }
    if (state.done) {
      then();
    } else {
      state.waiting.push(then);
    }
  }

  var chunks = document.getElementsByClassName("odr-chunk");

  function observed(entries) {
    entries.forEach(function (entry) {
      setNear(entry.target, entry.isIntersecting);
    });
  }

  // The document as the root, because a frame from another origin ignores the
  // margin of the implicit one; a browser that takes no document as a root
  // takes the implicit one.
  var near = null;
  if (typeof IntersectionObserver === "function") {
    try {
      near = new IntersectionObserver(observed, {
        root: document,
        rootMargin: "100% 0px",
      });
    } catch (e) {
      near = new IntersectionObserver(observed, { rootMargin: "100% 0px" });
    }
  }
  for (var i = 0; i < chunks.length; ++i) {
    if (near !== null && !chunks[i].hasAttribute("data-odr-chunk-held")) {
      near.observe(chunks[i]);
    } else {
      setNear(chunks[i], true);
    }
  }

  odr.chunks = {
    // Lays out the chunk that holds @p element, so it can be measured and
    // scrolled to. Observed anew, so the observer takes it back once it is
    // far, though it saw no change.
    show: function (element) {
      var chunk = element.closest(".odr-chunk");
      if (chunk !== null) {
        setNear(chunk, true);
        if (near !== null && !chunk.hasAttribute("data-odr-chunk-held")) {
          near.unobserve(chunk);
          near.observe(chunk);
        }
      }
    },
    // Puts the content of chunk @p index in, which its script states.
    fill: function (index, html) {
      var chunk = document.querySelector('[data-odr-chunk="' + index + '"]');
      if (chunk === null) {
        return;
      }
      chunk.innerHTML = html;
      delete texts[index];
      chunk.removeAttribute("data-odr-chunk-src");
      chunk.removeAttribute("data-odr-chunk-loading");
      chunk.dispatchEvent(new CustomEvent("odr-chunk-filled", { bubbles: true }));
    },
    // Keeps the text of the chunks that a script of the search index states
    // and that have not loaded since.
    text: function (byKey) {
      var fold = typeof odr.foldSearchText === "function" ? odr.foldSearchText : null;
      for (var key in byKey) {
        if (!Object.prototype.hasOwnProperty.call(byKey, key)) {
          continue;
        }
        var chunk = document.querySelector('[data-odr-chunk="' + key + '"]');
        if (chunk !== null && chunk.hasAttribute("data-odr-chunk-src")) {
          texts[key] = { text: fold === null ? byKey[key] : fold(byKey[key]), folded: fold !== null };
        }
      }
    },
    // The chunks that have not loaded, in document order, each with its text
    // or null and whether that is folded, once the index scripts have loaded.
    // The first chunk of a group names the script of the group, which is
    // loaded only while a chunk of the group has not.
    pending: function (then) {
      var waiting = [];
      var sources = [];
      var group = null;
      for (var i = 0; i < chunks.length; ++i) {
        group = chunks[i].getAttribute("data-odr-chunk-text") || group;
        if (chunks[i].hasAttribute("data-odr-chunk-src")) {
          waiting.push(chunks[i]);
          if (group !== null && sources.indexOf(group) === -1) {
            sources.push(group);
          }
        }
      }
      var left = sources.length + 1;
      function loaded() {
        if (--left > 0) {
          return;
        }
        then(
          waiting
            .filter(function (chunk) {
              return chunk.hasAttribute("data-odr-chunk-src");
            })
            .map(function (chunk) {
              var text = texts[chunk.getAttribute("data-odr-chunk")];
              return text === undefined
                ? { chunk: chunk, text: null, folded: false }
                : { chunk: chunk, text: text.text, folded: text.folded };
            })
        );
      }
      sources.forEach(function (source) {
        loadIndex(source, loaded);
      });
      loaded();
    },
    // Watches a chunk that a script added, as the view writes them.
    observe: function (chunk) {
      if (near !== null) {
        near.observe(chunk);
      } else {
        setNear(chunk, true);
      }
    },
  };
})();
