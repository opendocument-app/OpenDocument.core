// The text editor, attached to `odr.editing`. The mode makes the whole view
// editable and refuses what it cannot replay: only `setText` reaches the file,
// so an edit has to land inside one addressed run. `editing.md` phase 3
// replaces the collection with a model of its own.
(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});

  // A view whose runs carry no address is not this editor's: a sheet's editing
  // is an overlay, and its cells state no path.
  if (document.querySelector("[data-odr-path]") === null) {
    return;
  }

  var root = document.body;
  var modified = {};

  function operations() {
    var ops = [];
    for (var path in modified) {
      if (Object.prototype.hasOwnProperty.call(modified, path)) {
        ops.push({
          op: "setText",
          path: path,
          // Not `innerText`: that is the rendered text, and it drops the
          // trailing space a reader just typed.
          text: modified[path].textContent,
        });
      }
    }
    return ops;
  }

  /// The run @p node sits in, or null where it sits outside one - between two
  /// paragraphs, beside a picture, in the gap under the last block.
  function runOf(node) {
    var element = node === null || node.nodeType === 1 ? node : node.parentElement;
    return element === null ? null : element.closest("[data-odr-path]");
  }

  // The input types that only ever change the text of one run. Everything else
  // is refused, because `setText` is the only op the file takes.
  var textual = {
    insertText: 1,
    insertReplacementText: 1,
    insertFromPaste: 1,
    insertCompositionText: 1,
    deleteContent: 1,
    deleteContentBackward: 1,
    deleteContentForward: 1,
    deleteByCut: 1,
    deleteWordBackward: 1,
    deleteWordForward: 1,
    deleteSoftLineBackward: 1,
    deleteSoftLineForward: 1,
    // The browser's own stack holds the text edits above and nothing else,
    // because the structural ones never happened.
    historyUndo: 1,
    historyRedo: 1,
  };

  // A new line has nowhere to go in a run, and the reader knows the key.
  var named = { insertParagraph: "newLine", insertLineBreak: "newLine" };

  /// Where an edit lands: `run` is the one run it is confined to, null where
  /// it spans two or lands outside every run. `path` is where it starts, which
  /// is what a host needs to say *where* an edit was refused.
  function target(event) {
    var ranges =
      typeof event.getTargetRanges === "function" ? event.getTargetRanges() : [];
    var range = ranges.length === 1 ? ranges[0] : undefined;
    if (ranges.length === 0) {
      // No target range: a composition, and some browsers for a paste. The
      // caret is what the edit will land on.
      var selection = window.getSelection();
      if (selection !== null && selection.rangeCount > 0) {
        range = selection.getRangeAt(0);
      }
    }
    if (range === undefined) {
      return { run: null, path: null };
    }
    var start = runOf(range.startContainer);
    return {
      run: start !== null && start === runOf(range.endContainer) ? start : null,
      path: start === null ? null : start.getAttribute("data-odr-path"),
    };
  }

  function refuse(event, reason, at) {
    if (event.cancelable) {
      event.preventDefault();
    }
    // The path is what keeps two refusals apart, so a reader pressing Enter in
    // one run and then in another hears about both.
    odr.editing.refuse(reason, { path: at.path });
  }

  root.addEventListener("beforeinput", function (event) {
    var at = target(event);
    if (!odr.editing.isEnabled()) {
      refuse(event, "readOnly", at);
      return;
    }
    if (!textual[event.inputType]) {
      refuse(event, named[event.inputType] || "unsupportedEdit", at);
      return;
    }
    // Its own stack is what the browser replays; there is nothing to address.
    if (event.inputType === "historyUndo" || event.inputType === "historyRedo") {
      return;
    }
    if (at.run === null) {
      refuse(event, "range", at);
      return;
    }
    if (event.inputType === "insertFromPaste") {
      // Whatever the clipboard holds, one run takes plain text on one line.
      var text = event.dataTransfer
        ? event.dataTransfer.getData("text/plain")
        : null;
      if (text === null) {
        refuse(event, "unsupportedEdit", at);
      } else if (/[\r\n]/.test(text)) {
        refuse(event, "newLine", at);
      } else {
        event.preventDefault();
        document.execCommand("insertText", false, text);
      }
    }
  });

  new MutationObserver(function (mutations) {
    if (!odr.editing.isEnabled()) {
      return;
    }
    var moved = false;
    for (var i = 0; i < mutations.length; ++i) {
      if (mutations[i].type !== "characterData") {
        continue;
      }
      // The nearest owner, not the direct parent: a search `<mark>` may sit
      // between the edited text and the element carrying the path.
      var owner = runOf(mutations[i].target);
      if (owner !== null) {
        modified[owner.getAttribute("data-odr-path")] = owner;
        moved = true;
      } else {
        // The page changed where no op can name it. `beforeinput` should have
        // refused this, so a host hearing it has found a hole.
        odr.onError(9, "text changed outside an addressed run");
      }
    }
    if (moved) {
      odr.editing.changed();
    }
  }).observe(root, {
    childList: true,
    subtree: true,
    characterData: true,
  });

  odr.editing.attach({
    enable: function () {
      root.setAttribute("contenteditable", "true");
    },
    disable: function () {
      root.removeAttribute("contenteditable");
    },
    operations: operations,
    committed: function () {
      modified = {};
    },
  });
})();
