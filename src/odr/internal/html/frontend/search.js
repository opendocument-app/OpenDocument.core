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
