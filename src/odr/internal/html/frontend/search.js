(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});

  // The hits in document order: `{marks}` for a hit in the DOM, and
  // `{chunk, count}` for the hits of a chunk that has not loaded, which the
  // index counted (`lazy-loading.md` decision 4). `total` is the number of
  // hits, and `current` the place of the selected one among them, which is
  // `currentEntry` and the hit `currentOrdinal` of it.
  var hits = [];
  var total = 0;
  var current = -1;
  var currentEntry = null;
  var currentOrdinal = 0;
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

  // A character as `fold` takes it: a surrogate pair is one.
  var characters = /[\ud800-\udbff][\udc00-\udfff]|[^\x00-\x7f]/g;

  // `fold` with no map, for the text of a chunk: an ASCII character folds by
  // `toLowerCase` of the whole text, which is far faster. A `\n` is where the
  // text of a block ends, so it stays.
  function foldText(text) {
    return text.replace(characters, foldCharacter).replace(/\t/g, " ").toLowerCase();
  }

  // `viewport.js` keeps the index text folded, because only a search reads it.
  odr.foldSearchText = foldText;

  var ascii = /^[\x00-\x7f]*$/;

  // The folded `text` plus a folded-index to source-index map (with an end
  // sentinel), so a match maps back onto the source string. Folding per
  // character is what keeps that map right when a character folds to none or
  // to several. ASCII folds to as many characters, so its map is null: the
  // identity, which a page of a million lines would else hold an entry of
  // for each character.
  function fold(text) {
    if (ascii.test(text)) {
      return { text: text.replace(/[\t\n]/g, " ").toLowerCase(), map: null };
    }
    var folded = "";
    var map = [];
    for (var i = 0; i < text.length; ++i) {
      var unit = text.charCodeAt(i);
      var pair = unit >= 0xd800 && unit <= 0xdbff && i + 1 < text.length &&
        text.charCodeAt(i + 1) >= 0xdc00 && text.charCodeAt(i + 1) <= 0xdfff;
      var character = foldCharacter(pair ? text.substr(i, 2) : text[i]);
      for (var j = 0; j < character.length; ++j) {
        map.push(i);
      }
      folded += character;
      if (pair) {
        ++i;
      }
    }
    map.push(text.length);
    return { text: folded, map: map };
  }

  var inlineTags = {
    A: true, ABBR: true, B: true, BDI: true, BDO: true, BIG: true, BR: true,
    CITE: true, CODE: true, DATA: true, DEL: true, DFN: true, EM: true,
    FONT: true, I: true, IMG: true, INS: true, KBD: true, LABEL: true,
    MARK: true, Q: true, S: true, SAMP: true, SMALL: true, SPAN: true,
    STRONG: true, SUB: true, SUP: true, TIME: true, U: true, VAR: true,
    WBR: true, "X-S": true,
  };

  // Whether an element flows inline, from what states it: its own style, the
  // one class the frontend css makes a block of, its tag. Not
  // `getComputedStyle`, which computes the style of a whole chunk the browser
  // does not lay out: on a text file of 200,000 lines, a search took a
  // gigabyte and a minute. An element of another namespace, such as svg,
  // flows inline, and an unknown element too, as the browser has it.
  function isInline(element) {
    var display = element.style.display;
    if (display !== "") {
      return display.indexOf("inline") === 0 || display === "contents";
    }
    if (element.namespaceURI !== "http://www.w3.org/1999/xhtml") {
      return true;
    }
    if (element.classList.contains("odr-sheet-raised-box")) {
      return false;
    }
    var name = element.nodeName;
    return inlineTags[name] === true || (name.indexOf("-") !== -1 && name !== "X-P");
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
        var from = Math.max(at, piece.begin) - piece.begin;
        var to = Math.min(end, piece.end) - piece.begin;
        slices.push({
          node: piece.node,
          from: piece.map === null ? from : piece.map[from],
          to: piece.map === null ? to : piece.map[to],
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

  function weight(hit) {
    return hit.marks !== undefined ? 1 : hit.count;
  }

  // The place of the first hit of `entry` among all hits.
  function placeOf(entry) {
    var place = 0;
    for (var i = 0; i < hits.length && hits[i] !== entry; ++i) {
      place += weight(hits[i]);
    }
    return place;
  }

  // `total` and `current` again, after `hits` changed around the selected
  // entry, which stays selected.
  function recount() {
    total = 0;
    for (var i = 0; i < hits.length; ++i) {
      total += weight(hits[i]);
    }
    current = currentEntry === null ? -1 : placeOf(currentEntry) + currentOrdinal;
  }

  function resolveDone(found) {
    var waiting = done;
    done = [];
    waiting.forEach(function (resolve) {
      resolve(found);
    });
  }

  function notify() {
    if (typeof odr.onSearchChange === "function") {
      odr.onSearchChange({
        count: total,
        current: current,
        complete: complete,
      });
    }
    if (complete) {
      resolveDone(total);
    }
  }

  // Adds the hits that the text of the chunks that have not loaded holds.
  // Both lists are in document order, so they merge.
  function finish(pending) {
    var counted = [];
    pending.forEach(function (chunk) {
      if (chunk.text === null) {
        return;
      }
      var text = chunk.folded ? chunk.text : foldText(chunk.text);
      var found = count(text, keyword);
      if (found > 0) {
        counted.push({ chunk: chunk.chunk, count: found });
      }
    });
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
    recount();
    if (currentEntry === null && total > 0) {
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
    var at = -1;
    for (var i = 0; i < hits.length; ++i) {
      if (hits[i].chunk === chunk) {
        at = i;
        break;
      }
    }
    var ordinal = -1;
    if (at !== -1) {
      if (currentEntry === hits[at]) {
        ordinal = currentOrdinal;
        currentEntry = null;
      }
      hits.splice(at, 1);
    } else if (found.length === 0) {
      return;
    } else {
      // a chunk that loaded before the index counted it
      var high = hits.length;
      at = 0;
      while (at < high) {
        var middle = (at + high) >> 1;
        if (precedes(hits[middle], { chunk: chunk })) {
          at = middle + 1;
        } else {
          high = middle;
        }
      }
    }
    Array.prototype.splice.apply(hits, [at, 0].concat(found));
    recount();
    if (ordinal !== -1 && total > 0) {
      // the DOM states the hits of the chunk; the index only counted them
      var place = at < hits.length ? placeOf(hits[at]) : total - 1;
      var offset = Math.max(Math.min(ordinal, found.length - 1), 0);
      select(Math.min(place + offset, total - 1));
    }
    notify();
  });

  function select(index) {
    if (currentEntry !== null && currentEntry.marks !== undefined) {
      currentEntry.marks.forEach(function (mark) {
        mark.classList.remove("current");
      });
    }
    var place = 0;
    var i = 0;
    while (place + weight(hits[i]) <= index) {
      place += weight(hits[i++]);
    }
    current = index;
    currentEntry = hits[i];
    currentOrdinal = index - place;
    var hit = currentEntry;
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
    if (total === 0) {
      return 0;
    }
    select((current + delta + total) % total);
    notify();
    return total;
  }

  // A search that a new one replaces resolves its `searchDone` with null.
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
    total = 0;
    current = -1;
    currentEntry = null;
    currentOrdinal = 0;
    keyword = "";
    ++generation;
    complete = true;
    resolveDone(null);
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
    total = hits.length;
    if (total > 0) {
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
    return total;
  };

  // The count of the last search once it holds every chunk, or null where a
  // new search replaced it first.
  odr.searchDone = function () {
    return new Promise(function (resolve) {
      if (complete) {
        resolve(total);
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
