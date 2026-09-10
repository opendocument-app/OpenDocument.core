// The text editor, attached to `odr.editing`. It owns the edit: it cancels
// what the browser was about to do and splices the page itself, so the markup
// stays what the renderer wrote. See `docs/design/document-editing.md`.
(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});

  // A view whose runs carry no address is not this editor's: a sheet's editing
  // is an overlay, and its cells state no id.
  if (document.querySelector("x-s[data-odr-id]") === null) {
    return;
  }

  var root = document.body;

  // ---------------------------------------------------------------- address

  // ids for the elements this session creates; the render wrote the positive
  // ones, and replay tells the two apart by the sign
  var lastMinted = 0;

  function mint() {
    return --lastMinted;
  }

  function idOf(element) {
    return element === null ? null : Number(element.getAttribute("data-odr-id"));
  }

  /// The run @p node sits in, or null where it sits outside every run. A
  /// paragraph carries an address too, so the tag is part of the question.
  function runOf(node) {
    if (node === null || node === undefined) {
      return null;
    }
    var element = node.nodeType === 1 ? node : node.parentElement;
    return element === null ? null : element.closest("x-s[data-odr-id]");
  }

  function paragraphOf(node) {
    if (node === null || node === undefined) {
      return null;
    }
    var element = node.nodeType === 1 ? node : node.parentElement;
    return element === null ? null : element.closest("x-p[data-odr-id]");
  }

  /// The runs of @p paragraph, in document order - one under a span or a link
  /// counts as the paragraph's.
  function runsOf(paragraph) {
    return Array.prototype.slice.call(
      paragraph.querySelectorAll("x-s[data-odr-id]")
    );
  }

  // ------------------------------------------------------------------- page

  /// The `<br>` or `<wbr>` @p paragraph ends with, or null. A line break in
  /// the middle of one is a `<br>` too, so only the last child counts.
  function lineBoxOf(paragraph) {
    var last = paragraph.lastChild;
    return last !== null && (last.nodeName === "BR" || last.nodeName === "WBR")
      ? last
      : null;
  }

  /// The renderer ends a paragraph with `<br>` where it holds nothing and
  /// `<wbr>` where it holds something; keeping to that makes an edited page
  /// look like a freshly rendered one.
  function refreshLineBox(paragraph) {
    var box;
    while ((box = lineBoxOf(paragraph)) !== null) {
      paragraph.removeChild(box);
    }
    // a picture is content the same way text is, as the renderer has it
    var holds =
      paragraph.textContent !== "" || paragraph.firstElementChild !== null;
    paragraph.appendChild(document.createElement(holds ? "wbr" : "br"));
  }

  /// A copy of @p element carrying what it looks like and none of what it
  /// holds. @p id is null for a wrapper no operation names.
  function shellCopy(element, id) {
    var copy = element.cloneNode(false);
    if (id === null) {
      copy.removeAttribute("data-odr-id");
    } else {
      copy.setAttribute("data-odr-id", String(id));
    }
    return copy;
  }

  // -------------------------------------------------------------------- log

  // a step holds the operations it puts on the wire and the two halves of
  // taking it back; `done` is what a save reads, `undone` what redo replays
  var done = [];
  var undone = [];

  function perform(step) {
    if (step === null) {
      return null;
    }
    step.apply();
    done.push(step);
    undone.length = 0;
    odr.editing.changed();
    return step;
  }

  /// A copy of @p op carrying @p text: folding must not write into the
  /// operation its step hands over again after an undo and a redo.
  function withText(op, text) {
    var copy = {};
    for (var field in op) {
      if (Object.prototype.hasOwnProperty.call(op, field)) {
        copy[field] = op[field];
      }
    }
    copy.text = text;
    return copy;
  }

  /// Folds what a save need not carry: several edits to one run are the text
  /// it ends at, and a run created and typed into is one insert. Only adjacent
  /// operations fold, so nothing between them can depend on it.
  function coalesce(ops) {
    var result = [];
    for (var i = 0; i < ops.length; ++i) {
      var op = ops[i];
      var last = result.length === 0 ? null : result[result.length - 1];
      if (
        last !== null &&
        op.op === "setText" &&
        (last.op === "setText" || last.op === "insertText") &&
        last.id === op.id
      ) {
        result[result.length - 1] = withText(last, op.text);
        continue;
      }
      result.push(op);
    }
    return result;
  }

  function operations() {
    var ops = [];
    for (var i = 0; i < done.length; ++i) {
      for (var j = 0; j < done[i].ops.length; ++j) {
        ops.push(done[i].ops[j]);
      }
    }
    return coalesce(ops);
  }

  // -------------------------------------------------------------- mutations

  function setRunText(run, text) {
    var before = run.textContent;
    if (text === before) {
      return null; // an edit that changes nothing is not an operation
    }
    return {
      ops: [{ op: "setText", id: idOf(run), text: text }],
      apply: function () {
        run.textContent = text;
      },
      revert: function () {
        run.textContent = before;
      },
    };
  }

  /// A run beside @p anchor, cloned from it so it lands in the same parent -
  /// which is what gives it the same style on replay.
  function insertRun(anchor, where, text) {
    var id = mint();
    var run = shellCopy(anchor, id);
    run.textContent = text;
    var op = { op: "insertText", text: text, id: id };
    op[where] = idOf(anchor);
    return {
      run: run,
      ops: [op],
      apply: function () {
        anchor.parentNode.insertBefore(
          run,
          where === "after" ? anchor.nextSibling : anchor
        );
      },
      revert: function () {
        run.parentNode.removeChild(run);
      },
    };
  }

  /// The first run of a paragraph that holds none: what a reader types into
  /// after Enter.
  function appendRun(paragraph, text) {
    var id = mint();
    var run = document.createElement("x-s");
    run.setAttribute("data-odr-id", String(id));
    run.textContent = text;
    return {
      run: run,
      ops: [{ op: "insertText", parent: idOf(paragraph), text: text, id: id }],
      apply: function () {
        // ahead of the line box, which is the paragraph's last child
        paragraph.insertBefore(run, paragraph.lastChild);
        refreshLineBox(paragraph);
      },
      revert: function () {
        paragraph.removeChild(run);
        refreshLineBox(paragraph);
      },
    };
  }

  function removeElement(element) {
    var parent = element.parentNode;
    var at = element.nextSibling;
    return {
      ops: [{ op: "removeElement", id: idOf(element) }],
      apply: function () {
        parent.removeChild(element);
      },
      revert: function () {
        parent.insertBefore(element, at);
      },
    };
  }

  /// Splits @p element after @p stays - one of its children, or null to move
  /// all of them - into a copy of itself. The line box is the paragraph's own
  /// and stays out of the move.
  function splitLevel(element, stays, id, undoLog) {
    var copy = shellCopy(element, id);
    element.parentNode.insertBefore(copy, element.nextSibling);
    undoLog.push(function () {
      copy.parentNode.removeChild(copy);
    });

    // What moved, in order, rather than a sibling captured per node: the one
    // after the last of them is the line box, which `refreshLineBox` replaces.
    var moved = [];
    var node = stays === null ? element.firstChild : stays.nextSibling;
    while (node !== null) {
      var next = node.nextSibling;
      if (node.nodeName !== "BR" && node.nodeName !== "WBR") {
        moved.push(node);
        copy.appendChild(node);
      }
      node = next;
    }
    undoLog.push(function () {
      var box = lineBoxOf(element);
      for (var i = 0; i < moved.length; ++i) {
        element.insertBefore(moved[i], box);
      }
    });
    return copy;
  }

  /// Splits @p paragraph after @p after - one of its runs, or null to move
  /// everything. Every span and link on the way up is split too.
  /// No `after` splits before every child, stated as an absent key.
  function splitOp(paragraph, after, id) {
    var op = { op: "splitParagraph", paragraph: idOf(paragraph), id: id };
    if (after !== null) {
      op.after = idOf(after);
    }
    return op;
  }

  function splitParagraph(paragraph, after) {
    var id = mint();
    var undoLog = [];
    var tail = null;

    return {
      get tail() {
        return tail;
      },
      ops: [splitOp(paragraph, after, id)],
      apply: function () {
        undoLog = [];
        var stays = after;
        var level = after === null ? paragraph : after.parentNode;
        while (level !== paragraph) {
          splitLevel(level, stays, null, undoLog);
          stays = level;
          level = level.parentNode;
        }
        tail = splitLevel(paragraph, stays, id, undoLog);
        refreshLineBox(paragraph);
        refreshLineBox(tail);
      },
      revert: function () {
        for (var i = undoLog.length - 1; i >= 0; --i) {
          undoLog[i]();
        }
        undoLog = [];
        refreshLineBox(paragraph);
        tail = null;
      },
    };
  }

  /// @p paragraph takes the children of the paragraph after it, which goes.
  function mergeParagraph(paragraph) {
    var next = null;
    var undoLog = [];
    return {
      ops: [{ op: "mergeParagraph", paragraph: idOf(paragraph) }],
      apply: function () {
        undoLog = [];
        next = paragraph.nextElementSibling;
        var parent = next.parentNode;
        var at = next.nextSibling;
        undoLog.push(function () {
          parent.insertBefore(next, at);
        });

        var node = next.firstChild;
        while (node !== null) {
          var following = node.nextSibling;
          if (node.nodeName !== "BR" && node.nodeName !== "WBR") {
            (function (moved, from, back) {
              undoLog.push(function () {
                from.insertBefore(moved, back);
              });
            })(node, next, following);
            paragraph.insertBefore(node, paragraph.lastChild);
          }
          node = following;
        }
        parent.removeChild(next);
        refreshLineBox(paragraph);
      },
      revert: function () {
        for (var i = undoLog.length - 1; i >= 0; --i) {
          undoLog[i]();
        }
        undoLog = [];
        refreshLineBox(paragraph);
        refreshLineBox(next);
      },
    };
  }

  function insertParagraph(after) {
    var id = mint();
    var paragraph = shellCopy(after, id);
    paragraph.appendChild(document.createElement("br"));
    return {
      paragraph: paragraph,
      ops: [{ op: "insertParagraph", after: idOf(after), id: id }],
      apply: function () {
        after.parentNode.insertBefore(paragraph, after.nextSibling);
      },
      revert: function () {
        paragraph.parentNode.removeChild(paragraph);
      },
    };
  }

  // ------------------------------------------------------------------ caret

  function placeCaret(run, offset) {
    var node = run.firstChild;
    if (node === null || node.nodeType !== 3) {
      node = run.insertBefore(document.createTextNode(""), run.firstChild);
    }
    var range = document.createRange();
    range.setStart(node, Math.max(0, Math.min(offset, node.data.length)));
    range.collapse(true);
    var selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
  }

  /// The offset into @p run's text that (@p container, @p offset) names. The
  /// browser counts characters inside a text node and children inside an
  /// element, either of which may sit under a nested wrapper.
  function offsetInRun(run, container, offset) {
    var counted = 0;
    if (container.nodeType === 1) {
      var child = container.firstChild;
      for (var i = 0; i < offset && child !== null; ++i) {
        counted += child.textContent.length;
        child = child.nextSibling;
      }
    } else {
      counted = offset;
    }
    for (var node = container; node !== run; node = node.parentNode) {
      for (
        var previous = node.previousSibling;
        previous !== null;
        previous = previous.previousSibling
      ) {
        counted += previous.textContent.length;
      }
    }
    return counted;
  }

  /// Where an edit lands, in the terms an operation is written in: a run and
  /// an offset into its text. Null outside every run and every paragraph.
  function placeOf(container, offset) {
    var run = runOf(container);
    if (run !== null) {
      return {
        run: run,
        offset: offsetInRun(run, container, offset),
        paragraph: paragraphOf(run),
      };
    }

    var paragraph = paragraphOf(container);
    if (paragraph === null) {
      return null;
    }
    // beside the line box: the end of the last run is where a reader means,
    // and a paragraph holding none is where the first run goes
    var runs = runsOf(paragraph);
    if (runs.length === 0) {
      return { run: null, offset: 0, paragraph: paragraph };
    }
    var last = runs[runs.length - 1];
    return {
      run: last,
      offset: last.textContent.length,
      paragraph: paragraph,
    };
  }

  /// The range an event states, or the selection where it states none - which
  /// is what a browser lacking `getTargetRanges` leaves us.
  function rangeOf(event) {
    var ranges =
      typeof event.getTargetRanges === "function" ? event.getTargetRanges() : [];
    var range = ranges.length === 1 ? ranges[0] : undefined;
    if (range === undefined) {
      var selection = window.getSelection();
      if (selection === null || selection.rangeCount === 0) {
        return null;
      }
      range = selection.getRangeAt(0);
    }
    var start = placeOf(range.startContainer, range.startOffset);
    var end = placeOf(range.endContainer, range.endOffset);
    if (start === null || end === null) {
      return null;
    }
    return { start: start, end: end };
  }

  // ------------------------------------------------------------------ edits

  /// Replaces what @p at covers with @p text, and answers where the caret
  /// then sits. One run, several runs, or several paragraphs - the last case
  /// merges what is left of the two ends into one paragraph.
  function replaceRange(at, text) {
    var start = at.start;
    var end = at.end;

    // Everything the range covers has to be expressible before any of it is
    // applied, or a refusal would leave half an edit on the page.
    var between =
      start.paragraph === end.paragraph
        ? []
        : paragraphsBetween(start.paragraph, end.paragraph);
    if (between === null) {
      return null;
    }
    if (start.run !== end.run) {
      // the far end has to be a run, and what lies between has to be text
      if (start.run === null || end.run === null) {
        return null;
      }
      if (!coversOnlyText(start.run, end.run)) {
        return null;
      }
    }

    if (start.run === null) {
      if (text === "") {
        return start;
      }
      // a paragraph holding no run at all: the text opens one
      var opened = perform(appendRun(start.paragraph, text));
      return {
        run: opened.run,
        offset: text.length,
        paragraph: start.paragraph,
      };
    }

    var head = start.run.textContent.slice(0, start.offset);
    var caret = {
      run: start.run,
      offset: head.length + text.length,
      paragraph: start.paragraph,
    };

    if (start.run === end.run) {
      perform(
        setRunText(start.run, head + text + end.run.textContent.slice(end.offset))
      );
      return caret;
    }

    var tail = end.run.textContent.slice(end.offset);
    perform(setRunText(start.run, head + text));

    if (start.paragraph === end.paragraph) {
      removeRunsBetween(start.paragraph, start.run, end.run);
      perform(setRunText(end.run, tail));
      return caret;
    }

    // Several paragraphs: what is left of each end joins, and everything
    // between goes whole.
    removeRunsBetween(start.paragraph, start.run, null);
    removeRunsBetween(end.paragraph, null, end.run);
    perform(setRunText(end.run, tail));

    for (var i = 0; i < between.length; ++i) {
      perform(removeElement(between[i]));
    }
    perform(mergeParagraph(start.paragraph));

    return caret;
  }

  /// What @p paragraph holds that an operation can name, in document order:
  /// its runs, and anything a range takes away whole.
  function addressedIn(paragraph) {
    return Array.prototype.filter.call(
      paragraph.querySelectorAll("[data-odr-id]"),
      function (element) {
        return element.tagName === "X-S" || removableWhole(element);
      }
    );
  }

  /// Removes what @p paragraph holds strictly between @p after and @p before;
  /// a null end means from the first, or to the last.
  function removeRunsBetween(paragraph, after, before) {
    var held = addressedIn(paragraph);
    var from = after === null ? 0 : held.indexOf(after) + 1;
    var to = before === null ? held.length : held.indexOf(before);
    for (var i = from; i < to; ++i) {
      perform(removeElement(held[i]));
    }
  }

  // The text a range reaches over: a run, a wrapper around one, a paragraph of
  // them, and the line box.
  var reachable = { "X-S": 1, A: 1, "X-P": 1, BR: 1, WBR: 1 };

  /// Whether a range can take @p element away whole: it carries an address, so
  /// `removeElement` can name it, and holds no run to orphan. A picture is one;
  /// a text box is not.
  function removableWhole(element) {
    return (
      element.getAttribute("data-odr-id") !== null &&
      element.tagName !== "X-S" &&
      element.tagName !== "X-P" &&
      element.querySelector("x-s[data-odr-id]") === null
    );
  }

  /// Whether everything between @p from and @p to is text, or sits in
  /// something the range takes away whole.
  function coversOnlyText(from, to) {
    var probe = document.createRange();
    probe.setStartAfter(from);
    probe.setEndBefore(to);
    var fragment = probe.cloneContents();
    var nodes = fragment.querySelectorAll("*");
    for (var i = 0; i < nodes.length; ++i) {
      if (!reaches(nodes[i], fragment)) {
        return false;
      }
    }
    return true;
  }

  /// Whether @p element is text, or sits in something taken away whole.
  function reaches(element, fragment) {
    if (reachable[element.tagName] === 1) {
      return true;
    }
    for (var at = element; at !== null && at !== fragment; at = at.parentNode) {
      if (removableWhole(at)) {
        return true;
      }
    }
    return false;
  }

  /// The paragraphs strictly between @p first and @p last, or null where
  /// something that is not a paragraph lies between them - a table, or a
  /// drawing anchored beside them rather than inside one.
  function paragraphsBetween(first, last) {
    if (first.parentNode !== last.parentNode) {
      return null;
    }
    var result = [];
    for (
      var at = first.nextElementSibling;
      at !== null && at !== last;
      at = at.nextElementSibling
    ) {
      if (at.tagName !== "X-P" || at.getAttribute("data-odr-id") === null) {
        return null;
      }
      result.push(at);
    }
    return result;
  }

  /// Enter: what the caret covers goes, and the paragraph splits where it
  /// then sits. Answers where the caret lands, which is the head of the new
  /// paragraph.
  function splitAt(at) {
    var caret = replaceRange(at, "");
    if (caret === null) {
      return null;
    }
    var paragraph = caret.paragraph;

    if (caret.run === null) {
      var empty = perform(insertParagraph(paragraph));
      return { paragraph: empty.paragraph, run: null, offset: 0 };
    }

    var after = caret.run;
    if (caret.offset >= caret.run.textContent.length) {
      // the caret is at the end of its run: nothing has to be cut
    } else if (caret.offset === 0) {
      var runs = runsOf(paragraph);
      var before = runs.indexOf(caret.run) - 1;
      after = before < 0 ? null : runs[before];
    } else {
      var whole = caret.run.textContent;
      perform(setRunText(caret.run, whole.slice(0, caret.offset)));
      perform(insertRun(caret.run, "after", whole.slice(caret.offset)));
    }

    var split = perform(splitParagraph(paragraph, after));
    var tailRuns = runsOf(split.tail);
    return {
      paragraph: split.tail,
      run: tailRuns.length === 0 ? null : tailRuns[0],
      offset: 0,
    };
  }

  /// Puts the caret where an edit left it; a paragraph holding no run has
  /// nowhere but itself.
  function restore(caret) {
    if (caret.run !== null) {
      placeCaret(caret.run, caret.offset);
      return;
    }
    var range = document.createRange();
    range.setStart(caret.paragraph, 0);
    range.collapse(true);
    var selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
  }

  // ------------------------------------------------------------------- gate

  // Every edit is one of two shapes: a range replaced by some text, or a
  // paragraph split where the caret sits. `text` reads what to put in.
  var replacing = {
    insertText: function (event) {
      return event.data === null ? "" : event.data;
    },
    insertReplacementText: function (event) {
      return event.data === null ? "" : event.data;
    },
    deleteContent: empty,
    deleteContentBackward: empty,
    deleteContentForward: empty,
    deleteByCut: empty,
    deleteWordBackward: empty,
    deleteWordForward: empty,
    deleteSoftLineBackward: empty,
    deleteSoftLineForward: empty,
    deleteHardLineBackward: empty,
    deleteHardLineForward: empty,
    deleteEntireSoftLine: empty,
  };

  function empty() {
    return "";
  }

  // no operation carries a soft line break
  var named = { insertLineBreak: "newLine" };

  // A delete whose range the browser did not state is one character, in the
  // direction the key names. The word and line deletes are not on this list:
  // guessing where a word ends would take away text the reader did not name.
  var extending = { deleteContentBackward: -1, deleteContentForward: 1 };

  /// The run before @p run, reaching into the paragraph before where it is
  /// the first - which is what Backspace at a paragraph start does.
  function runBefore(run) {
    var paragraph = paragraphOf(run);
    var runs = runsOf(paragraph);
    var at = runs.indexOf(run);
    if (at > 0) {
      return { run: runs[at - 1], paragraph: paragraph };
    }
    var previous = paragraph.previousElementSibling;
    if (previous === null || previous.tagName !== "X-P") {
      return null;
    }
    var before = runsOf(previous);
    return before.length === 0
      ? null
      : { run: before[before.length - 1], paragraph: previous };
  }

  function runAfter(run) {
    var paragraph = paragraphOf(run);
    var runs = runsOf(paragraph);
    var at = runs.indexOf(run);
    if (at + 1 < runs.length) {
      return { run: runs[at + 1], paragraph: paragraph };
    }
    var next = paragraph.nextElementSibling;
    if (next === null || next.tagName !== "X-P") {
      return null;
    }
    var after = runsOf(next);
    return after.length === 0 ? null : { run: after[0], paragraph: next };
  }

  /// Grows a collapsed range by the one character a delete key takes, or by
  /// the paragraph boundary it stands at - which merges and takes no
  /// character. Null where there is nothing to take.
  function extendForDelete(at, direction) {
    if (at.start.run !== at.end.run || at.start.offset !== at.end.offset) {
      return at;
    }
    var place = at.start;
    if (place.run === null) {
      return null;
    }
    if (direction < 0) {
      if (place.offset > 0) {
        return {
          start: { run: place.run, offset: place.offset - 1, paragraph: place.paragraph },
          end: place,
        };
      }
      var before = runBefore(place.run);
      if (before === null) {
        return null;
      }
      // at the start of a paragraph the boundary itself is what goes, so the
      // range takes no character with it
      var back = before.paragraph === place.paragraph ? 1 : 0;
      return {
        start: {
          run: before.run,
          offset: before.run.textContent.length - back,
          paragraph: before.paragraph,
        },
        end: place,
      };
    }
    if (place.offset < place.run.textContent.length) {
      return {
        start: place,
        end: { run: place.run, offset: place.offset + 1, paragraph: place.paragraph },
      };
    }
    var after = runAfter(place.run);
    if (after === null) {
      return null;
    }
    var forward = after.paragraph === place.paragraph ? 1 : 0;
    return {
      start: place,
      end: { run: after.run, offset: forward, paragraph: after.paragraph },
    };
  }

  function refuse(event, reason, at) {
    if (event.cancelable) {
      event.preventDefault();
    }
    // the id keeps two refusals apart, so the same key in one run and then in
    // another is heard twice
    odr.editing.refuse(reason, {
      id: at === null || at.start.run === null ? null : idOf(at.start.run),
    });
  }

  // a composition cannot be cancelled, so the browser writes and we read the
  // run back afterwards; this is the run it started in
  var composing = null;

  root.addEventListener("compositionstart", function () {
    var selection = window.getSelection();
    composing =
      selection === null || selection.rangeCount === 0
        ? null
        : runOf(selection.getRangeAt(0).startContainer);
  });

  root.addEventListener("compositionend", function () {
    var run = composing;
    composing = null;
    if (!odr.editing.isEnabled()) {
      return;
    }
    var selection = window.getSelection();
    var landed =
      selection === null || selection.rangeCount === 0
        ? null
        : runOf(selection.getRangeAt(0).startContainer);
    var target = landed !== null ? landed : run;
    if (target === null) {
      odr.onError(9, "an edit landed where no operation can name it");
      return;
    }
    // whatever the browser built inside the run, its text is the operation
    perform(setRunText(target, target.textContent));
  });

  root.addEventListener("beforeinput", function (event) {
    var type = event.inputType;
    var at = rangeOf(event);

    if (!odr.editing.isEnabled()) {
      refuse(event, "readOnly", at);
      return;
    }

    if (type === "historyUndo" || type === "historyRedo") {
      event.preventDefault();
      if (type === "historyUndo") {
        odr.editing.undo();
      } else {
        odr.editing.redo();
      }
      return;
    }

    // mid-composition and unstoppable; `compositionend` reconciles it
    if (type === "insertCompositionText" || composing !== null) {
      return;
    }

    if (at === null) {
      refuse(event, "range", at);
      return;
    }

    if (type === "insertParagraph") {
      var split = splitAt(at);
      if (split === null) {
        refuse(event, "range", at);
        return;
      }
      event.preventDefault();
      restore(split);
      return;
    }

    if (type === "insertFromPaste") {
      var pasted = event.dataTransfer
        ? event.dataTransfer.getData("text/plain")
        : null;
      if (pasted === null) {
        refuse(event, "unsupportedEdit", at);
        return;
      }
      if (!paste(at, pasted)) {
        refuse(event, "range", at);
        return;
      }
      event.preventDefault();
      return;
    }

    var text = replacing[type];
    if (text === undefined) {
      refuse(event, named[type] || "unsupportedEdit", at);
      return;
    }

    var covering = extending[type] === undefined
        ? at
        : extendForDelete(at, extending[type]);
    if (covering === null) {
      // nothing to take: the key does nothing rather than being refused
      event.preventDefault();
      return;
    }

    var caret = replaceRange(covering, text(event));
    if (caret === null) {
      refuse(event, "range", at);
      return;
    }
    event.preventDefault();
    restore(caret);
  });

  /// A paste is its lines: the first replaces the selection, each one after
  /// it opens a paragraph.
  function paste(at, pasted) {
    var lines = pasted.split(/\r\n|\r|\n/);
    var caret = replaceRange(at, lines[0]);
    if (caret === null) {
      return false;
    }
    for (var i = 1; i < lines.length; ++i) {
      caret = splitAt({ start: caret, end: caret });
      if (caret !== null && lines[i] !== "") {
        caret = replaceRange({ start: caret, end: caret }, lines[i]);
      }
      if (caret === null) {
        // the lines before this one stand; a host replays onto a fresh decode
        return false;
      }
    }
    restore(caret);
    return true;
  }

  // ------------------------------------------------------------------ mode

  odr.editing.attach({
    enable: function () {
      root.setAttribute("contenteditable", "true");
    },
    disable: function () {
      root.removeAttribute("contenteditable");
    },
    operations: operations,
    canUndo: function () {
      return done.length > 0;
    },
    canRedo: function () {
      return undone.length > 0;
    },
    undo: function () {
      if (done.length === 0) {
        return false;
      }
      var step = done.pop();
      step.revert();
      undone.push(step);
      odr.editing.changed();
      return true;
    },
    redo: function () {
      if (undone.length === 0) {
        return false;
      }
      var step = undone.pop();
      step.apply();
      done.push(step);
      odr.editing.changed();
      return true;
    },
    committed: function () {
      done.length = 0;
      undone.length = 0;
    },
  });
})();
