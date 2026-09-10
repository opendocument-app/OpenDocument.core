// The text editor, attached to `odr.editing`. The mode makes the whole view
// editable and refuses what it cannot replay: `setText` is the only op the
// file takes, so an edit has to land inside one addressed run.
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

  /// The run @p node sits in, or null where it sits outside every run.
  function runOf(node) {
    if (node === null) {
      return null;
    }
    var element = node.nodeType === 1 ? node : node.parentElement;
    return element === null ? null : element.closest("[data-odr-path]");
  }

  // The input types that only ever change the text of one run.
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
    // Its stack holds the edits above and nothing else: the structural ones
    // never happened.
    historyUndo: 1,
    historyRedo: 1,
  };

  // A new line has nowhere to go in a run, and the reader knows the key.
  var named = { insertParagraph: "newLine", insertLineBreak: "newLine" };

  /// Where an edit lands: `run` is the one run it is confined to, null where
  /// it spans two or lands outside every run. `path` is where it starts.
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
    // The path keeps two refusals apart, so Enter in one run and then in
    // another is heard twice.
    odr.editing.refuse(reason, { path: at.path });
  }

  // Where the edit the gate just allowed will land, for `input` to record.
  var pending = null;

  root.addEventListener("beforeinput", function (event) {
    pending = null;
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
    pending = at.run;
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

  /// The run the caret sits in, for an edit no `beforeinput` announced.
  function selectedRun() {
    var selection = window.getSelection();
    return selection === null || selection.rangeCount === 0
      ? null
      : runOf(selection.getRangeAt(0).startContainer);
  }

  // `input` is the browser saying it applied an edit, which a script rewriting
  // the page never raises - so a search highlighting nine matches leaves the
  // log alone. A `MutationObserver` could not tell the two apart.
  root.addEventListener("input", function () {
    if (!odr.editing.isEnabled()) {
      return;
    }
    // The caret first: the browser has just put it where the edit landed. A
    // `beforeinput` whose edit changed nothing raises no `input`, so what it
    // left in `pending` may be a run ago - it is the fallback, not the answer.
    var run = selectedRun();
    if (run === null) {
      run = pending;
    }
    pending = null;
    if (run === null) {
      // The gate refuses an edit it cannot name, so this is a hole in it: a
      // WebView that raised no `beforeinput`, or a composition it cannot stop.
      odr.onError(9, "an edit landed where no operation can name it");
      return;
    }
    modified[run.getAttribute("data-odr-path")] = run;
    odr.editing.changed();
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
