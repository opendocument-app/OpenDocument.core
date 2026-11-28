// Progressive page projection. The source DOM never becomes a page fragment.
(function () {
  "use strict";
  var odr = (window.odr = window.odr || {}),
    view = odr.documentView;
  var root = document.querySelector("[data-odr-pagination]");
  if (!root || !view) return;
  var original = view.source;
  var textMap = view.textMap,
    range = view.range,
    boundary = view.boundary;
  var templates = Object.create(null),
    sources = new WeakMap();
  root
    .querySelectorAll("template[data-odr-page-template]")
    .forEach(function (t) {
      templates[t.getAttribute("data-odr-page-template")] = t;
    });
  var options = {
    enabled: root.getAttribute("data-odr-pagination") === "true",
    explicitBreaks: root.getAttribute("data-odr-page-breaks") !== "false",
    showPageBorders: root.getAttribute("data-odr-page-borders") !== "false",
    animate: root.getAttribute("data-odr-page-animation") !== "false",
  };
  var column = document.createElement("div"),
    pending = document.createElement("div");
  column.className = "odr-pages odr-pagination-column";
  pending.className = "odr-pagination-pending";
  column.appendChild(pending);
  root.insertBefore(column, original);
  var queue = [],
    pages = [],
    index = 0,
    oversized = 0;
  var current = null,
    frame = 0,
    timer = 0,
    active = false,
    pendingBreak = false;
  var restartKey = "root",
    searching = false,
    printing = false,
    restartPending = false,
    replacingPages = 0;
  var wasColumn = root.classList.contains("odr-pages");
  var wasBackdrop =
    wasColumn && document.body.classList.contains("odr-background");
  var loadedImages = new Set();

  function state() {
    return {
      enabled: options.enabled,
      active: active,
      complete: active && index === queue.length,
      pageCount: pages.length,
      processedBlocks: index,
      totalBlocks: queue.length,
      oversizedPages: oversized,
    };
  }
  function report() {
    var detail = state();
    document.dispatchEvent(
      new CustomEvent("odr-pagination-change", { detail: detail }),
    );
    if (typeof odr.onPaginationChange === "function")
      odr.onPaginationChange(detail);
  }
  function nodes() {
    var result = [];
    original
      .querySelectorAll(".odr-text-flow, .odr-page-inner")
      .forEach(function (container) {
        var content =
          container.querySelector(":scope > .odr-page-body") || container;
        Array.prototype.forEach.call(content.childNodes, function (node) {
          if (
            node.nodeType === 1 &&
            !node.matches(
              ".odr-page-header, .odr-page-footer, style, script, template",
            )
          )
            result.push(node);
        });
      });
    return result;
  }
  // Legacy fallback page boxes must not become paragraph boundaries in the model.
  function normalizeSource() {
    if (original.querySelector(".odr-text-flow")) return;
    var flow = document.createElement("div");
    flow.className = "odr-text-flow";
    original
      .querySelectorAll(".odr-page-inner")
      .forEach(function (inner, group) {
        var content = inner.querySelector(":scope > .odr-page-body") || inner;
        var children = Array.from(content.children).filter(function (n) {
          return !n.matches(".odr-page-header, .odr-page-footer");
        });
        if (group && children.length)
          children[0].setAttribute("data-odr-flow-break", "");
        children.forEach(function (n) {
          flow.appendChild(n);
        });
      });
    original.replaceChildren(flow);
    wasColumn = false;
    wasBackdrop = false;
  }
  function collect() {
    return nodes().map(function (node) {
      var item = sources.get(node);
      if (!item) {
        item = { node: node, offset: 0, views: [], tail: null };
        sources.set(node, item);
      }
      return item;
    });
  }
  function fragment(item, start, end) {
    var clone;
    if (start === 0 && end === item.map.length)
      clone = item.node.cloneNode(true);
    else {
      var selected = range(item.map, start, end),
        contents = selected.cloneContents();
      var ancestor = selected.commonAncestorContainer;
      if (ancestor.nodeType === 3) ancestor = ancestor.parentNode;
      while (ancestor !== item.node) {
        var wrapper = ancestor.cloneNode(false);
        wrapper.appendChild(contents);
        contents = wrapper;
        ancestor = ancestor.parentNode;
      }
      clone = item.node.cloneNode(false);
      clone.appendChild(contents);
    }
    clone.setAttribute("data-odr-fragment", "");
    if (start > 0) {
      clone.style.marginTop = "0";
      clone.style.textIndent = "0";
    }
    if (end < item.map.length) clone.style.marginBottom = "0";
    return view.bind(clone, item.node, start, end);
  }
  function tail(item, offset) {
    if (item.tail) item.tail.remove();
    item.tail = null;
    if (offset < item.map.length || (offset === 0 && item.map.length === 0)) {
      item.tail = fragment(item, offset, item.map.length);
      pending.appendChild(item.tail);
    }
  }
  function accept(item, clone, end) {
    item.views.push(clone);
    if (end === item.map.length) {
      if (item.tail) item.tail.remove();
      item.tail = null;
    } else if (item.tail) {
      range(textMap(item.tail), 0, end - item.offset).deleteContents();
      view.bind(item.tail, item.node, end, item.map.length);
      item.tail.style.marginTop = "0";
      item.tail.style.textIndent = "0";
    }
    item.offset = end;
    ++current.count;
    item.needsPage = end < item.map.length;
  }
  function fits() {
    return current.body.scrollHeight <= current.body.clientHeight + 1;
  }
  /// The first offset in @p map whose glyph ends below @p limit.
  function firstAfter(map, limit) {
    var low = 0,
      high = map.length;
    while (low < high) {
      var middle = (low + high) >> 1;
      var rect = range(map, 0, middle + 1).getBoundingClientRect();
      if (rect.bottom <= limit + 0.1) low = middle + 1;
      else high = middle;
    }
    return low;
  }
  function paragraph(item) {
    if (!item.map.length) return false;
    var start = item.offset,
      end = Math.min(item.map.length, start + 512),
      shown;
    while (true) {
      shown = fragment(item, start, end);
      current.body.appendChild(shown);
      if (!fits() || end === item.map.length) break;
      shown.remove();
      end = Math.min(item.map.length, start + (end - start) * 2);
    }
    if (!fits()) {
      var map = textMap(shown);
      var cut = firstAfter(map, current.body.getBoundingClientRect().bottom);
      if (cut < map.length) {
        // Different fonts can have different glyph bottoms on the same line.
        var overflowing = range(map, cut, cut + 1).getBoundingClientRect();
        cut = firstAfter(map, overflowing.top);
      }
      // A glyph can fit while its line box does not. Retain only whole lines.
      while (cut > 0) {
        var last = range(map, cut - 1, cut).getBoundingClientRect();
        var candidate = fragment(item, start, start + cut);
        shown.replaceWith(candidate);
        if (fits()) {
          shown = candidate;
          end = start + cut;
          break;
        }
        candidate.replaceWith(shown);
        var earlier = firstAfter(map, last.top);
        // Zero-height glyphs may offer no earlier line boundary.
        cut = earlier < cut ? earlier : 0;
      }
      if (cut === 0 || end === start) {
        shown.remove();
        if (current.count > 0) {
          item.needsPage = true;
          return true;
        }
        // Even one line, or the paragraph's fixed box, is taller than a page.
        return false;
      }
    }
    accept(item, shown, end);
    return true;
  }
  function viewportRect(node) {
    return odr.getViewportRect
      ? odr.getViewportRect(node)
      : node.getBoundingClientRect();
  }
  function readingPosition() {
    var scrollLeft = window.scrollX,
      scrollTop = window.scrollY;
    var x = window.innerWidth / 2,
      y = Math.min(80, window.innerHeight / 4);
    var caret = document.caretPositionFromPoint
      ? document.caretPositionFromPoint(x, y)
      : null;
    var r =
      !caret && document.caretRangeFromPoint
        ? document.caretRangeFromPoint(x, y)
        : null;
    var node = caret ? caret.offsetNode : r && r.startContainer;
    var offset = caret ? caret.offset : r && r.startOffset;
    var point = node && view.sourcePoint(node, offset);
    function top() {
      var at = point && projectedPoint(point);
      if (!at || pending.contains(at[0])) return null;
      var probe = document.createRange();
      probe.setStart(at[0], at[1]);
      probe.collapse(true);
      var rect = viewportRect(probe);
      return rect.height ? rect.top : null;
    }
    var before = top();
    return function () {
      var after = top();
      var targetTop =
        before !== null && after !== null
          ? window.scrollY + after - before
          : scrollTop;
      if (
        Math.abs(window.scrollY - targetTop) > 1 ||
        window.scrollX !== scrollLeft
      )
        window.scrollTo({
          left: scrollLeft,
          top: targetTop,
          behavior: "instant",
        });
    };
  }
  function projectedPoint(point) {
    if (!point.element || !original.contains(point.element)) return null;
    var block = point.element;
    while (block && !sources.has(block)) block = block.parentElement;
    var item = block && sources.get(block);
    if (!item) return null;
    var positions = item.views.concat(item.tail ? [item.tail] : []),
      fallback = null;
    for (var i = 0; i < positions.length; ++i) {
      var shown = positions[i];
      if (!shown.isConnected) continue;
      var candidates = [shown].concat(
        Array.from(shown.querySelectorAll("[data-odr-source-id]")),
      );
      for (var j = 0; j < candidates.length; ++j) {
        var record = view.mapping(candidates[j]);
        if (
          !record ||
          record.source !== point.element ||
          point.offset < record.start ||
          point.offset > record.end
        )
          continue;
        var result = boundary(
          textMap(candidates[j]),
          point.offset - record.start,
        );
        if (
          (point.offset === record.end && point.affinity !== "upstream") ||
          (point.offset === record.start && point.affinity === "upstream")
        )
          fallback = result;
        else return result;
      }
    }
    return fallback;
  }
  function pause() {
    cancelAnimationFrame(frame);
    frame = 0;
    clearTimeout(timer);
  }
  function stop() {
    if (!active) return;
    if (view.finishComposition) view.finishComposition();
    pause();
    var selection = view.capture();
    active = false;
    pages.forEach(function (p) {
      p.node.remove();
    });
    pages = [];
    current = null;
    oversized = 0;
    pending.replaceChildren();
    column.style.removeProperty("min-height");
    queue.forEach(function (item) {
      item.views = [];
      item.tail = null;
    });
    original.hidden = false;
    original.removeAttribute("contenteditable");
    original.removeAttribute("aria-hidden");
    original
      .querySelectorAll("[data-odr-source-bookmark]")
      .forEach(function (n) {
        n.id = n.getAttribute("data-odr-source-bookmark");
        n.removeAttribute("data-odr-source-bookmark");
      });
    root.classList.remove("odr-pagination-active");
    root.classList.toggle("odr-pages", wasColumn);
    document.body.classList.toggle("odr-background", wasBackdrop);
    view.discardRecords();
    view.restore(selection);
  }
  function start() {
    if (view.isComposing()) {
      restartPending = true;
      return;
    }
    restartPending = false;
    var selection = view.capture();
    stop();
    if (!options.enabled || printing || searching) {
      report();
      return;
    }
    normalizeSource();
    original.querySelectorAll("[id]").forEach(function (n) {
      n.setAttribute("data-odr-source-bookmark", n.id);
      n.removeAttribute("id");
    });
    queue = collect();
    index = 0;
    pendingBreak = false;
    restartKey = "root";
    replacingPages = 0;
    queue.forEach(function (item) {
      item.map = textMap(item.node);
      item.text = item.node.textContent;
      item.offset = 0;
      item.views = [];
      item.needsPage = false;
      item.after = undefined;
      tail(item, 0);
    });
    active = true;
    original.hidden = true;
    original.setAttribute("contenteditable", "false");
    original.setAttribute("aria-hidden", "true");
    root.classList.remove("odr-pages");
    root.classList.add("odr-pagination-active");
    decorations();
    view.discardRecords();
    view.restore(selection);
    schedule();
    report();
  }
  function decorations() {
    root.classList.toggle("odr-pagination-borders", options.showPageBorders);
    root.classList.toggle("odr-pagination-animate", options.animate);
    if (active)
      document.body.classList.toggle("odr-background", options.showPageBorders);
  }
  function page(key) {
    var template = templates[key] || templates.root;
    var node = template.content.firstElementChild.cloneNode(true);
    if (replacingPages > 0) {
      node.style.animation = "none";
      --replacingPages;
    }
    node
      .querySelectorAll("[data-odr-marginal]")
      .forEach(function (placeholder) {
        var marginal = root.querySelector(
          'template[data-odr-marginal-template="' +
            placeholder.getAttribute("data-odr-marginal") +
            '"]',
        );
        if (marginal) placeholder.replaceWith(marginal.content.cloneNode(true));
      });
    node
      .querySelectorAll(".odr-pagination-header, .odr-pagination-footer")
      .forEach(function (margin) {
        margin.setAttribute("contenteditable", "false");
      });
    node.querySelectorAll("[id]").forEach(function (n) {
      n.removeAttribute("id");
    });
    column.insertBefore(node, pending);
    current = {
      node: node,
      body: node.querySelector(".odr-pagination-body"),
      layout: template.getAttribute("data-odr-page-template"),
      next: template.getAttribute("data-odr-next-layout"),
      master: template.getAttribute("data-odr-master-page"),
      count: 0,
      oversized: false,
      checkpoint: {
        item: queue[index],
        offset: queue[index] ? queue[index].offset : 0,
        pendingBreak: pendingBreak,
      },
    };
    pages.push(current);
    pending.style.width = current.body.offsetWidth + "px";
    return current;
  }
  function force(value) {
    return /^(page|always|left|right|recto|verso)$/.test(value);
  }
  function near() {
    return (
      current === null ||
      viewportRect(current.node).bottom < window.innerHeight * 3
    );
  }
  function schedule() {
    if (
      !active ||
      frame ||
      view.isComposing() ||
      (current !== null && index === queue.length)
    )
      return;
    frame = requestAnimationFrame(function () {
      step();
    });
  }
  function step(restorePosition) {
    frame = 0;
    if (!active || view.isComposing() || !near()) return;
    var selection = view.capture();
    restorePosition = restorePosition || readingPosition();
    var deadline = performance.now() + 6,
      steps = 0;
    if (current === null) page(restartKey);
    while (index < queue.length && near() && steps++ < 24) {
      var item = queue[index],
        node = item.node,
        style = getComputedStyle(node);
      if (item.after === undefined) item.after = force(style.breakAfter);
      var master = !item.offset && node.getAttribute("data-odr-master-page");
      var marker = node.hasAttribute("data-odr-page-break");
      var breaks =
        options.explicitBreaks &&
        (pendingBreak ||
          node.hasAttribute("data-odr-flow-break") ||
          force(style.breakBefore));
      if (item.needsPage) {
        page(current.next);
        item.needsPage = false;
      } else if (!marker && (master || breaks || current.oversized)) {
        if (current.count > 0)
          page(
            master
              ? master + (master === current.master ? "" : "-first")
              : current.next,
          );
        else if (
          master &&
          master !== current.master &&
          templates[master + "-first"]
        ) {
          current.node.remove();
          pages.pop();
          page(master + "-first");
        }
      }
      var simple =
        node.nodeName === "X-P" &&
        !node.hasAttribute("data-odr-source-bookmark") &&
        !node.querySelector(
          'img, svg, table, [data-odr-source-bookmark], [contenteditable="false"], [data-odr-frame]',
        );
      if (simple && paragraph(item)) {
        if (item.offset === item.map.length) {
          pendingBreak = options.explicitBreaks && item.after;
          ++index;
        } else pendingBreak = false;
      } else if (marker) {
        if (item.tail) item.tail.remove();
        item.tail = null;
        pendingBreak = options.explicitBreaks;
        ++index;
      } else {
        var clone = fragment(item, item.offset, item.map.length);
        current.body.appendChild(clone);
        var inFlow =
          style.position !== "absolute" && style.position !== "fixed";
        if (inFlow && !fits()) {
          if (current.count > 0) {
            clone.remove();
            page(current.next).body.appendChild(clone);
          }
          if (!fits()) {
            current.oversized = true;
            ++oversized;
            current.node.style.minHeight = current.node.offsetHeight + "px";
            current.node.style.height = "auto";
            current.body.style.flex = "0 0 auto";
            current.node.setAttribute("data-odr-oversized", "");
          }
        }
        accept(item, clone, item.map.length);
        if (!inFlow) --current.count;
        pendingBreak = options.explicitBreaks && item.after;
        ++index;
      }
      if (performance.now() >= deadline) break;
    }
    view.restore(selection);
    restorePosition();
    if (
      index === queue.length ||
      viewportRect(current.node).bottom >= window.innerHeight
    )
      column.style.removeProperty("min-height");
    report();
    if (index < queue.length && near()) schedule();
  }
  function update(records, selection) {
    if (restartPending) {
      start();
      return;
    }
    if (!active || !records.length) {
      schedule();
      return;
    }
    // Removing page boxes can immediately clamp the browser's scroll offset.
    var restorePosition = readingPosition();
    // Keep the scroll range until budgeted reflow has rebuilt the visible pages.
    column.style.minHeight = column.offsetHeight + "px";
    pause();
    var changed = new Set(),
      old = queue,
      next = collect(),
      nextSet = new Set(next),
      earliest = old.length;
    var textOnly = true;
    function touch(node) {
      for (
        var n = node.nodeType === 1 ? node : node.parentNode;
        n && n !== original;
        n = n.parentNode
      ) {
        var item = sources.get(n);
        if (item) {
          changed.add(item);
          var at = old.indexOf(item);
          if (at >= 0) earliest = Math.min(earliest, at);
          return;
        }
      }
    }
    records.forEach(function (r) {
      if (
        r.type !== "characterData" &&
        !(
          r.type === "childList" &&
          r.target.nodeName === "X-S" &&
          Array.from(r.addedNodes)
            .concat(Array.from(r.removedNodes))
            .every(function (n) {
              return n.nodeType === 3;
            })
        )
      )
        textOnly = false;
      var target = r.target.nodeType === 1 ? r.target : r.target.parentElement;
      if (
        target &&
        (target.closest("style") ||
          (r.type === "attributes" &&
            (target === original || target.matches(".odr-text-flow"))))
      )
        earliest = 0;
      touch(r.target);
      Array.from(r.removedNodes || []).forEach(touch);
      Array.from(r.addedNodes || []).forEach(touch);
    });
    for (var i = 0; i < Math.max(old.length, next.length); ++i) {
      if (old[i] !== next[i]) {
        earliest = Math.min(earliest, i);
        break;
      }
    }
    var pageIndex = pages.length - 1;
    if (old[earliest] && old[earliest].views.length) {
      var affected = old[earliest],
        firstView = affected.views[0];
      if (textOnly && !affected.node.querySelector("br")) {
        var before = affected.text,
          after = affected.node.textContent,
          prefix = 0;
        while (
          prefix < before.length &&
          prefix < after.length &&
          before[prefix] === after[prefix]
        )
          ++prefix;
        var fragmentIndex = affected.views.findIndex(function (n) {
          return view.mapping(n).end >= prefix;
        });
        if (fragmentIndex >= 0) firstView = affected.views[fragmentIndex];
      }
      var first = firstView.closest(".odr-pagination-page");
      pageIndex = pages.findIndex(function (p) {
        return p.node === first;
      });
    }
    // A smaller block may now fit in the unused space on the preceding page.
    pageIndex = Math.max(0, pageIndex - 1);
    while (pageIndex > 0 && next.indexOf(pages[pageIndex].checkpoint.item) < 0)
      --pageIndex;
    var checkpoint = pages[pageIndex]
      ? pages[pageIndex].checkpoint
      : { item: next[0], offset: 0, pendingBreak: false };
    restartKey = pages[pageIndex] ? pages[pageIndex].layout : "root";
    index = Math.max(0, next.indexOf(checkpoint.item));
    replacingPages = pages.length - pageIndex;
    pages.splice(pageIndex).forEach(function (p) {
      p.node.remove();
    });
    oversized = pages.filter(function (p) {
      return p.oversized;
    }).length;
    old.forEach(function (item) {
      item.views = item.views.filter(function (n) {
        return n.isConnected;
      });
      if (!nextSet.has(item) && item.tail) item.tail.remove();
    });
    queue = next;
    current = null;
    pendingBreak = checkpoint.pendingBreak;
    for (var j = index; j < queue.length; ++j) {
      var item = queue[j],
        offset = j === index ? checkpoint.offset : 0;
      item.needsPage = false;
      item.after = undefined;
      if (changed.has(item)) item.map = null;
      if (!item.map) {
        item.map = textMap(item.node);
        item.text = item.node.textContent;
      }
      if (!item.tail || item.offset !== offset || changed.has(item))
        tail(item, offset);
      item.offset = offset;
    }
    var following = null;
    for (var k = queue.length - 1; k >= index; --k) {
      var remaining = queue[k].tail;
      if (!remaining) continue;
      if (remaining.nextSibling !== following)
        pending.insertBefore(remaining, following);
      following = remaining;
    }
    // Render the affected visible area now; distant pages remain frame-budgeted.
    step(restorePosition);
    view.restore(selection);
    schedule();
  }
  function invalidate() {
    if (!active) return;
    clearTimeout(timer);
    timer = setTimeout(start, 100);
  }
  view.connect({
    active: function () {
      return active;
    },
    point: projectedPoint,
    update: update,
    pause: pause,
  });
  odr.pagination = {
    getOptions: function () {
      return Object.assign({}, options);
    },
    getState: state,
    setOptions: function (value) {
      Object.keys(value).forEach(function (key) {
        if (
          !Object.prototype.hasOwnProperty.call(options, key) ||
          typeof value[key] !== "boolean"
        )
          throw new TypeError("invalid pagination option: " + key);
      });
      var reflow =
        (value.enabled !== undefined && value.enabled !== options.enabled) ||
        (value.explicitBreaks !== undefined &&
          value.explicitBreaks !== options.explicitBreaks);
      Object.assign(options, value);
      decorations();
      if (reflow) start();
      return state();
    },
    refresh: start,
    getPages: function () {
      return pages.map(function (p) {
        return {
          layout: p.layout,
          width: p.node.offsetWidth,
          height: p.node.offsetHeight,
          oversized: p.oversized,
        };
      });
    },
  };
  document.addEventListener("odr-search-state", function (event) {
    searching = event.detail.active;
    if (searching) {
      stop();
      report();
    } else timer = setTimeout(start, 0);
  });
  function copy(event) {
    var s = window.getSelection();
    if (!active || !s || s.isCollapsed || !event.clipboardData) return;
    var selected = view.inputRange(s.getRangeAt(0));
    if (!selected) return;
    var contents = selected.cloneContents();
    var plainFragment = contents.cloneNode(true);
    plainFragment.querySelectorAll("br").forEach(function (br) {
      br.replaceWith("\n");
    });
    var paragraphs = plainFragment.querySelectorAll("x-p");
    var plain = paragraphs.length
      ? Array.from(paragraphs)
          .map(function (p) {
            return p.textContent;
          })
          .join("\n")
      : plainFragment.textContent;
    var holder = document.createElement("div");
    holder.appendChild(contents);
    holder
      .querySelectorAll("[data-odr-id], [data-odr-source-bookmark]")
      .forEach(function (n) {
        n.removeAttribute("data-odr-id");
        n.removeAttribute("data-odr-source-bookmark");
      });
    event.clipboardData.setData("text/plain", plain);
    event.clipboardData.setData("text/html", holder.innerHTML);
    event.preventDefault();
    return true;
  }
  document.addEventListener("copy", copy);
  document.addEventListener("cut", function (event) {
    if (!odr.editing || !odr.editing.isEnabled() || !copy(event)) return;
    document.body.dispatchEvent(
      new InputEvent("beforeinput", {
        bubbles: true,
        cancelable: true,
        inputType: "deleteByCut",
      }),
    );
  });
  window.addEventListener("scroll", schedule, { passive: true });
  window.addEventListener("resize", invalidate);
  root.addEventListener(
    "load",
    function (event) {
      var image = event.target;
      if (image.tagName !== "IMG") return;
      var key =
        image.currentSrc + ":" + image.naturalWidth + ":" + image.naturalHeight;
      if (!loadedImages.has(key)) {
        loadedImages.add(key);
        invalidate();
      }
    },
    true,
  );
  if (document.fonts) {
    document.fonts.ready.then(invalidate);
    document.fonts.addEventListener("loadingdone", invalidate);
  }
  window.addEventListener("beforeprint", function () {
    printing = true;
    stop();
    report();
  });
  window.addEventListener("afterprint", function () {
    printing = false;
    start();
  });
  start();
})();
