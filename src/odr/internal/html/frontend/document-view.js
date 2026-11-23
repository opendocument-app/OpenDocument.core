// Maps temporary page fragments to the document editor's authoritative DOM.
(function () {
  "use strict";
  var odr = (window.odr = window.odr || {});
  var source = document.querySelector(".odr-pagination-original");
  if (!source) return;
  var projection = null,
    depth = 0,
    desired = null,
    composing = null,
    queued = [];
  var mappings = new WeakMap(),
    elements = new Map();
  source.querySelectorAll("[data-odr-id]").forEach(function (node) {
    elements.set(node.getAttribute("data-odr-id"), node);
  });
  var observer = new MutationObserver(function (records) {
    if (!active()) return;
    queued.push.apply(queued, records);
    if (!depth && !composing) flush([]);
  });
  observer.observe(source, {
    subtree: true,
    childList: true,
    characterData: true,
    attributes: true,
  });

  function active() {
    return projection !== null && projection.active();
  }
  function textMap(node) {
    var entries = [],
      length = 0;
    var walker = document.createTreeWalker(
      node,
      NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT,
    );
    for (var n = walker.nextNode(); n; n = walker.nextNode()) {
      var size = n.nodeType === 3 ? n.length : n.nodeName === "BR" ? 1 : 0;
      if (size) {
        entries.push({ node: n, start: length, end: length + size });
        length += size;
      }
    }
    return { root: node, entries: entries, length: length };
  }
  function boundary(map, offset) {
    if (offset <= 0) return [map.root, 0];
    if (offset >= map.length) return [map.root, map.root.childNodes.length];
    var low = 0,
      high = map.entries.length - 1;
    while (low < high) {
      var middle = (low + high) >> 1;
      if (map.entries[middle].end <= offset) low = middle + 1;
      else high = middle;
    }
    var entry = map.entries[low],
      n = entry.node;
    return n.nodeType === 3
      ? [n, offset - entry.start]
      : [
          n.parentNode,
          Array.prototype.indexOf.call(n.parentNode.childNodes, n),
        ];
  }
  function range(map, start, end) {
    var r = document.createRange(),
      a = boundary(map, start),
      b = boundary(map, end);
    r.setStart(a[0], a[1]);
    r.setEnd(b[0], b[1]);
    return r;
  }
  function offsetIn(root, node, offset) {
    var r = document.createRange();
    r.selectNodeContents(root);
    r.setEnd(node, offset);
    return textMap(r.cloneContents()).length;
  }
  function address(id) {
    var element = elements.get(id);
    if (!element || !source.contains(element)) {
      element = source.querySelector('[data-odr-id="' + id + '"]');
      if (element) elements.set(id, element);
    }
    return element;
  }
  function sourcePoint(node, offset, affinity) {
    if (!node) return null;
    var element = node.nodeType === 1 ? node : node.parentElement;
    if (source.contains(node)) {
      var addressed =
        element && element.closest("x-s[data-odr-id], x-p[data-odr-id]");
      var owner = addressed || (element && element.closest("x-p"));
      return owner
        ? {
            element: owner,
            offset: offsetIn(owner, node, offset),
            affinity: affinity || "downstream",
          }
        : { node: node, offset: offset };
    }
    for (
      var view = element;
      view && !mappings.has(view);
      view = view.parentElement
    ) {}
    if (!view) {
      if (!active()) return { node: node, offset: offset };
      if (node.nodeType !== 1) return null;
      function edge(child, end) {
        if (child.nodeType !== 1) return null;
        if (mappings.has(child))
          return sourcePoint(
            child,
            end ? child.childNodes.length : 0,
            end ? "upstream" : "downstream",
          );
        var candidates = Array.from(
          child.querySelectorAll("[data-odr-fragment], [data-odr-source-id]"),
        );
        if (end) candidates.reverse();
        var mapped = candidates.find(function (n) {
          return mappings.has(n);
        });
        return mapped
          ? sourcePoint(
              mapped,
              end ? mapped.childNodes.length : 0,
              end ? "upstream" : "downstream",
            )
          : null;
      }
      // Native selections can end between page-body children, outside a run.
      for (var i = offset; i < node.childNodes.length; ++i) {
        var following = edge(node.childNodes[i], false);
        if (following) return following;
      }
      for (var j = offset - 1; j >= 0; --j) {
        var preceding = edge(node.childNodes[j], true);
        if (preceding) return preceding;
      }
      return null;
    }
    var record = mappings.get(view);
    var local = offsetIn(view, node, offset);
    var point = boundary(textMap(record.source), record.start + local);
    return sourcePoint(
      point[0],
      point[1],
      affinity || (local === textMap(view).length ? "upstream" : "downstream"),
    );
  }
  function sourceBoundary(point) {
    if (!point) return null;
    return point.element
      ? boundary(textMap(point.element), point.offset)
      : point.node && point.node.isConnected
        ? [point.node, point.offset]
        : null;
  }
  function readSelection() {
    if (desired) return desired;
    var s = window.getSelection();
    if (!s || !s.anchorNode) return null;
    var a = sourcePoint(s.anchorNode, s.anchorOffset),
      b = sourcePoint(s.focusNode, s.focusOffset);
    return a && b ? { anchor: a, focus: b } : null;
  }
  function viewBoundary(point) {
    if (!active()) return sourceBoundary(point);
    if (!point.element) return null;
    return projection.point(point);
  }
  function restore(selection) {
    if (!selection) return;
    desired = selection;
    if (depth || composing) return;
    var a = viewBoundary(selection.anchor),
      b = viewBoundary(selection.focus);
    if (a && b) {
      window.getSelection().setBaseAndExtent(a[0], a[1], b[0], b[1]);
      desired = null;
    }
  }
  function flush(records) {
    queued.push.apply(queued, records);
    if (!active() || composing) return;
    records = queued;
    queued = [];
    var selection = readSelection();
    projection.update(records, selection);
    restore(selection);
  }
  function bind(view, original, start, end) {
    mappings.set(view, { source: original, start: start, end: end });
    var nodes = [view].concat(
      Array.from(view.querySelectorAll("[data-odr-id], [data-odr-source-id]")),
    );
    nodes.forEach(function (n) {
      var id =
        n.getAttribute("data-odr-id") || n.getAttribute("data-odr-source-id");
      if (!id) return;
      var model = address(id);
      if (model) {
        var from = model === original ? 0 : offsetIn(original, model, 0);
        mappings.set(n, {
          source: model,
          start: Math.max(0, start - from),
          end: Math.min(textMap(model).length, end - from),
        });
        n.setAttribute("data-odr-source-id", id);
      }
      n.removeAttribute("data-odr-id");
    });
    [view]
      .concat(Array.from(view.querySelectorAll("[data-odr-source-bookmark]")))
      .forEach(function (n) {
        if (n.hasAttribute("data-odr-source-bookmark")) {
          n.id = n.getAttribute("data-odr-source-bookmark");
          n.removeAttribute("data-odr-source-bookmark");
        }
      });
    return view;
  }
  function compositionStart() {
    if (!active() || composing) return false;
    var s = window.getSelection();
    var node = s && s.anchorNode;
    var element = node && (node.nodeType === 1 ? node : node.parentElement);
    var run = element && element.closest("x-s[data-odr-source-id]");
    var record = run && mappings.get(run);
    if (!record) return false;
    // Browsers insert beside an empty inline element, inside its paragraph.
    var paragraph = run.closest("x-p");
    composing = {
      view:
        record.source.textContent === "" &&
        paragraph &&
        paragraph.textContent === ""
          ? paragraph
          : run,
      source: record.source,
      prefix: record.source.textContent.slice(0, record.start),
      suffix: record.source.textContent.slice(record.end),
    };
    projection.pause();
    return true;
  }
  function compositionSync() {
    if (!composing || !composing.view.isConnected) return;
    var value =
      composing.prefix + composing.view.textContent + composing.suffix;
    if (composing.source.textContent !== value)
      composing.source.textContent = value;
    mappings.get(composing.view).end =
      composing.prefix.length + composing.view.textContent.length;
  }
  odr.documentView = {
    source: source,
    active: active,
    textMap: textMap,
    boundary: boundary,
    range: range,
    offsetIn: offsetIn,
    bind: bind,
    mapping: function (node) {
      return mappings.get(node);
    },
    sourcePoint: sourcePoint,
    sourceBoundary: sourceBoundary,
    capture: readSelection,
    restore: restore,
    connect: function (renderer) {
      projection = renderer;
    },
    begin: function () {
      ++depth;
    },
    end: function () {
      if (--depth === 0) {
        flush(observer.takeRecords());
        if (desired) restore(desired);
      }
    },
    discardRecords: function () {
      observer.takeRecords();
      queued = [];
    },
    select: function (r) {
      restore({
        anchor: sourcePoint(r.startContainer, r.startOffset),
        focus: sourcePoint(r.endContainer, r.endOffset),
      });
    },
    inputRange: function (r) {
      if (!active()) return r;
      var a = sourceBoundary(sourcePoint(r.startContainer, r.startOffset));
      var b = sourceBoundary(sourcePoint(r.endContainer, r.endOffset));
      if (!a || !b) return null;
      var result = document.createRange();
      result.setStart(a[0], a[1]);
      result.setEnd(b[0], b[1]);
      return result;
    },
    compositionStart: compositionStart,
    compositionSync: compositionSync,
    isComposing: function () {
      return composing !== null;
    },
    compositionEnd: function () {
      if (!composing) return;
      compositionSync();
      var selection = readSelection();
      composing = null;
      desired = selection;
      flush(observer.takeRecords());
    },
  };
})();
