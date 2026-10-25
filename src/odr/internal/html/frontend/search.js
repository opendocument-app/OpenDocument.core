(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});

  // Every hit in document order: the marks of a hit in the DOM, or the chunk
  // and the ordinal in it of a hit in a chunk that has not loaded
  // (`lazy-loading.md` decision 4).
  var hits = [];
  var current = -1;
  var keyword = "";
  var complete = true;
  var generation = 0;
  var done = [];

  var foldedCharacters = {};

  // Case- and diacritic-folded: one character at a time, so the fold of a
  // text is the folds of its characters. Every space folds to one: the page
  // holds a tab as is, and a keyword is typed without one.
  function foldCharacter(character) {
    var folded = foldedCharacters[character];
    if (folded === undefined) {
      folded = foldedCharacters[character] = character
        .normalize("NFD")
        .replace(/[\u0300-\u036f]/g, "")
        .replace(/[\t\n\u00a0\u2000-\u200a\u202f\u205f\u3000]/g, " ")
        .toLowerCase();
    }
    return folded;
  }

  // `fold` with no map, for the text of a chunk: an ASCII character folds by
  // `toLowerCase` of the whole text, which is far faster. A `\n` is where the
  // text of a block ends, so it stays.
  function foldText(text) {
    return text
      .replace(/[^\x00-\x7f]/g, foldCharacter)
      .replace(/\t/g, " ")
      .toLowerCase();
  }

  // The folded `text` plus a folded-index to source-index map (with an end
  // sentinel), so a match maps back onto the source string. Folding per
  // character is what keeps that map right when a character folds to none or
  // to several.
  function fold(text) {
    var folded = "";
    var map = [];
    for (var i = 0; i < text.length; ++i) {
      var character = foldCharacter(text[i]);
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
  // glyph layer out of a search of the same page's text layer. The numbers and
  // letters of a sheet label its rows and columns; they are not its content.
  function textNodes(root) {
    var walker = document.createTreeWalker(
      root,
      NodeFilter.SHOW_ELEMENT | NodeFilter.SHOW_TEXT,
      {
        acceptNode: function (node) {
          if (node.nodeType !== Node.TEXT_NODE) {
            var name = node.nodeName;
            return name === "SCRIPT" ||
              name === "STYLE" ||
              name === "MARK" ||
              node.getAttribute("aria-hidden") === "true" ||
              (name === "TH" &&
                (node.classList.contains("odr-sheet-row-header") ||
                  node.classList.contains("odr-sheet-column-header")))
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
  function blocks(root) {
    var nodes = textNodes(root);
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

  // How often `needle` is in `text`, counted as `findIn` counts.
  function count(text, needle) {
    var found = 0;
    for (
      var at = text.indexOf(needle);
      at !== -1;
      at = text.indexOf(needle, at + needle.length)
    ) {
      ++found;
    }
    return found;
  }

  // Marks every occurrence under `root` and returns the hits in reading order.
  function markIn(root) {
    var result = [];
    blocks(root).forEach(function (block) {
      var found = findIn(block, keyword);
      // Wrapped back to front: an offset still to be used sits before the split
      // that would move it. The marks are collected back into reading order.
      var wrapped = [];
      for (var i = found.length - 1; i >= 0; --i) {
        var marks = [];
        for (var j = found[i].length - 1; j >= 0; --j) {
          var slice = found[i][j];
          marks.unshift(wrap(slice.node, slice.from, slice.to));
        }
        wrapped.unshift({ marks: marks });
      }
      result = result.concat(wrapped);
    });
    return result;
  }

  function nodeOf(hit) {
    return hit.marks !== undefined ? hit.marks[0] : hit.chunk;
  }

  function precedes(a, b) {
    var position = nodeOf(a).compareDocumentPosition(nodeOf(b));
    return (position & Node.DOCUMENT_POSITION_FOLLOWING) !== 0;
  }

  function notify() {
    if (typeof odr.onSearchChange === "function") {
      odr.onSearchChange({
        count: hits.length,
        current: current,
        complete: complete,
      });
    }
    if (complete) {
      var waiting = done;
      done = [];
      waiting.forEach(function (resolve) {
        resolve(hits.length);
      });
    }
  }

  // Adds the hits that the text of the chunks that have not loaded holds.
  // Both lists are in document order, so they merge.
  function finish(pending) {
    var counted = [];
    pending.forEach(function (chunk) {
      var found =
        chunk.text === null ? 0 : count(foldText(chunk.text), keyword);
      for (var ordinal = 0; ordinal < found; ++ordinal) {
        counted.push({ chunk: chunk.chunk, ordinal: ordinal });
      }
    });
    var selected = hits[current];
    var merged = [];
    var i = 0;
    var j = 0;
    while (i < hits.length || j < counted.length) {
      var fromHits =
        j === counted.length ||
        (i < hits.length && precedes(hits[i], counted[j]));
      merged.push(fromHits ? hits[i++] : counted[j++]);
    }
    hits = merged;
    complete = true;
    if (selected !== undefined) {
      current = hits.indexOf(selected);
    } else if (hits.length > 0) {
      select(0);
    }
    notify();
  }

  // A chunk that loads while a keyword is searched for: its hits move from
  // the index into the DOM, and a hit selected in it is selected there.
  document.addEventListener("odr-chunk-filled", function (event) {
    if (keyword === "") {
      return;
    }
    var chunk = event.target;
    var found = markIn(chunk);
    var kept = [];
    var at = -1;
    var ordinal = -1;
    var selected;
    hits.forEach(function (hit, index) {
      if (hit.chunk === chunk) {
        if (at === -1) {
          at = kept.length;
        }
        if (index === current) {
          ordinal = hit.ordinal;
        }
      } else {
        if (index === current) {
          selected = hit;
        }
        kept.push(hit);
      }
    });
    if (at === -1) {
      if (found.length === 0) {
        return;
      }
      // a chunk that loaded before the index counted it
      var high = kept.length;
      at = 0;
      while (at < high) {
        var middle = (at + high) >> 1;
        if (precedes(kept[middle], { chunk: chunk })) {
          at = middle + 1;
        } else {
          high = middle;
        }
      }
    }
    hits = kept.slice(0, at).concat(found, kept.slice(at));
    current = -1;
    if (ordinal !== -1 && hits.length > 0) {
      // the DOM states the hits of the chunk; the index only counted them
      var offset = Math.max(Math.min(ordinal, found.length - 1), 0);
      select(Math.min(at + offset, hits.length - 1));
    } else if (selected !== undefined) {
      current = hits.indexOf(selected);
    }
    notify();
  });

  function select(index) {
    if (current >= 0 && hits[current] && hits[current].marks !== undefined) {
      hits[current].marks.forEach(function (mark) {
        mark.classList.remove("current");
      });
    }
    current = index;
    var hit = hits[current];
    if (hit.marks === undefined) {
      // selected once its chunk loads
      odr.chunks.show(hit.chunk);
      hit.chunk.scrollIntoView({ block: "center", inline: "center" });
      return;
    }
    hit.marks.forEach(function (mark) {
      mark.classList.add("current");
    });
    // A hit inside a folded section, or in a chunk the browser does not lay
    // out, is shown before it is scrolled to.
    for (
      var element = hit.marks[0].parentElement;
      element !== null;
      element = element.parentElement
    ) {
      if (element.nodeName === "DETAILS") {
        element.open = true;
      }
    }
    if (odr.chunks !== undefined) {
      odr.chunks.show(hit.marks[0]);
    }
    hit.marks[0].scrollIntoView({ block: "center", inline: "center" });
  }

  function step(delta, next) {
    if (next !== undefined && next !== null && fold(String(next)).text !== keyword) {
      return odr.search(next);
    }
    if (hits.length === 0) {
      return 0;
    }
    select((current + delta + hits.length) % hits.length);
    notify();
    return hits.length;
  }

  function reset() {
    hits.forEach(function (hit) {
      (hit.marks || []).forEach(function (mark) {
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
    hits = [];
    current = -1;
    keyword = "";
    ++generation;
    complete = true;
  }

  odr.resetSearch = function () {
    reset();
    notify();
  };

  // Highlights every occurrence, selects the first and returns the count. An
  // occurrence is one hit however many nodes it is written across. The count
  // is what is known at once: the hits of the chunks that have not loaded
  // come with `odr.onSearchChange` and `odr.searchDone`.
  odr.search = function (text) {
    reset();
    keyword = fold(text === undefined || text === null ? "" : String(text)).text;
    if (keyword === "") {
      notify();
      return 0;
    }
    hits = markIn(document.body);
    if (hits.length > 0) {
      select(0);
    }
    complete = false;
    var searched = generation;
    var finishing = function (pending) {
      if (searched === generation) {
        finish(pending);
      }
    };
    if (odr.chunks !== undefined && typeof odr.chunks.pending === "function") {
      odr.chunks.pending(finishing);
    } else {
      finishing([]);
    }
    if (!complete) {
      notify();
    }
    return hits.length;
  };

  // The count of the last search once it holds every chunk.
  odr.searchDone = function () {
    return new Promise(function (resolve) {
      if (complete) {
        resolve(hits.length);
      } else {
        done.push(resolve);
      }
    });
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
