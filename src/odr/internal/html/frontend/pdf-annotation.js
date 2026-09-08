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
