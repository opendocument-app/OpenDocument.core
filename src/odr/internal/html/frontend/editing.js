// `odr.editing`: the editing mode, generic over the formats. A format's editor
// attaches to it. See `docs/design/editing.md` decisions 9 to 12.
(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});
  var body = document.body;

  // An absent `data-odr-editable` is a render offering no editing, and answers
  // `readOnly` like a document that refuses one.
  var editable = body.getAttribute("data-odr-editable") === "true";
  var keyClasses = (body.getAttribute("data-odr-keyboard") || "").split(" ");

  var editing = false;
  var editors = [];
  var lastRefusal = null;

  // The codes are `odr::ErrorCode`, written into the page ahead of this
  // script. The message stays here: it is for a console, and nothing in this
  // library is localised.
  var codes = (odr.errorCodes = odr.errorCodes || {});
  var messages = {
    newLine: "a line break inside a paragraph is not supported",
    formula: "cell holds a formula",
    rich: "cell holds more than text",
    shapes: "cell holds a drawing",
    readOnly: "document cannot be edited",
    formulaInput: "typing a formula is not supported",
    unsupportedEdit: "this kind of edit is not supported",
    range: "an edit cannot reach over a picture or a table",
    unnameableEdit: "an edit landed where no operation can name it",
    outOfScope: "the edit reaches past what this page offers",
    sheetCut: "the sheet reaches past what this page renders",
  };

  /// Falls back to `readOnly` for a reason no script here states.
  function refusal(reason) {
    var known = Object.prototype.hasOwnProperty.call(messages, reason)
      ? reason
      : "readOnly";
    return { code: codes[known] || 0, message: messages[known] };
  }

  odr.onError = function (code, message) {
    console.error("error " + code + " message " + message);
  };
  odr.onEditRefused = function (event) {
    console.warn("edit refused " + event.code + ": " + event.message);
  };
  odr.onEditModeChange = function (event) {
    console.log("editing " + (event.editing ? "on" : "off"));
  };
  odr.onEditChange = function () {};
  odr.onCellsStale = function () {};
  odr.onSelectionChange = function () {};

  function fire(name, event) {
    if (typeof odr[name] === "function") {
      odr[name](event);
    }
  }

  /// Whether the scripts take a class of key event: `navigation` or
  /// `shortcuts`. `HtmlConfig` decides.
  odr.takesKeys = function (name) {
    return keyClasses.indexOf(name) !== -1;
  };

  /// Whether any editor answered @p name; the one attached last is asked first.
  function ask(name) {
    for (var i = editors.length - 1; i >= 0; --i) {
      var editor = editors[i];
      if (typeof editor[name] === "function" && editor[name]()) {
        return true;
      }
    }
    return false;
  }

  function tell(name) {
    for (var i = 0; i < editors.length; ++i) {
      if (typeof editors[i][name] === "function") {
        editors[i][name]();
      }
    }
  }

  /// What every editor would hand a save, in the order they attached.
  function operations() {
    var ops = [];
    for (var i = 0; i < editors.length; ++i) {
      var mine = editors[i].operations();
      for (var j = 0; j < mine.length; ++j) {
        ops.push(mine[j]);
      }
    }
    return ops;
  }

  /// Hands @p name to the last editor that has it, refusing where none does.
  function delegate(name, argument) {
    for (var i = editors.length - 1; i >= 0; --i) {
      if (typeof editors[i][name] === "function") {
        return editors[i][name](argument) === true;
      }
    }
    odr.editing.refuse("unsupportedEdit", null);
    return false;
  }

  function modeChange(reason) {
    var refused = reason ? refusal(reason) : null;
    fire("onEditModeChange", {
      editing: editing,
      editable: editable,
      reason: reason || null,
      code: refused ? refused.code : 0,
      message: refused ? refused.message : "",
    });
  }

  odr.editing = {
    /// False where nothing on this page can be edited, with the reason on
    /// `onEditModeChange` - so a host can grey its button before a click.
    enable: function () {
      if (!editable) {
        modeChange("readOnly");
        return false;
      }
      if (!editing) {
        editing = true;
        body.classList.add("odr-editing");
        tell("enable");
        modeChange(null);
      }
      return true;
    },
    disable: function () {
      if (editing) {
        editing = false;
        tell("disable");
        body.classList.remove("odr-editing");
        modeChange(null);
      }
    },
    isEnabled: function () {
      return editing;
    },
    /// Whether `enable` would succeed.
    isEditable: function () {
      return editable;
    },
    /// `paragraph` or `document`, as `<body>` states it. Read per edit, so a
    /// host can widen it without a render.
    scope: function () {
      return body.getAttribute("data-odr-editing-scope") === "paragraph"
        ? "paragraph"
        : "document";
    },

    /// Registers an editor; only `operations` is required.
    attach: function (editor) {
      editors.push(editor);
    },

    /// Formats the selection; false if refused. Text accepts `bold`, `italic`,
    /// `underline`, `strikethrough` (booleans), `size` (e.g. `14pt`), `color`
    /// and `highlight`. Paragraphs/cells accept `align`: left, center, right,
    /// justify, or null for cell-type alignment. Cells accept `fill`.
    /// Colors use `#rrggbb`; null clears highlight/fill.
    format: function (style) {
      return delegate("format", style);
    },

    /// Flips `bold`, `italic`, `underline` or `strikethrough` on the
    /// selection; a mixed selection turns on. On a collapsed caret at a word
    /// boundary the mark waits for the next typed text, and
    /// `onSelectionChange` shows it meanwhile.
    toggle: function (property) {
      return delegate("toggle", property);
    },

    /// Inserts as many rows as the selection spans, `"above"` it (the
    /// default) or `"below"` it. False where refused.
    insertRows: function (where) {
      return delegate("insertRows", where);
    },

    /// Removes the rows the selection spans. False where refused.
    deleteRows: function () {
      return delegate("deleteRows");
    },

    /// Inserts as many columns as the selection spans, `"left"` of it (the
    /// default) or `"right"` of it. False where refused.
    insertColumns: function (where) {
      return delegate("insertColumns", where);
    },

    /// Removes the columns the selection spans. False where refused.
    deleteColumns: function () {
      return delegate("deleteColumns");
    },

    /// The style the selection shows, a key per property the covered runs
    /// agree on; raised by an editor as the selection moves.
    selectionChanged: function (style) {
      fire("onSelectionChange", style);
    },

    /// Reports a refused edit, dropping a repeat of the same one within two
    /// seconds: four taps on a locked cell are one snackbar. @p detail is how
    /// the format addresses it. Painting it is the editor's.
    refuse: function (reason, detail) {
      var refused = refusal(reason);
      var key = reason + ":" + JSON.stringify(detail || null);
      var now = Date.now();
      if (lastRefusal !== null && lastRefusal.key === key && now - lastRefusal.at < 2000) {
        return;
      }
      lastRefusal = { key: key, at: now };
      var event = { reason: reason, code: refused.code, message: refused.message };
      for (var field in detail) {
        if (Object.prototype.hasOwnProperty.call(detail, field)) {
          event[field] = detail[field];
        }
      }
      fire("onEditRefused", event);
    },

    /// The cells the edits so far left computing an old input. Raised
    /// whenever the set changes, which an undo does too.
    stale: function (detail) {
      fire("onCellsStale", detail);
    },

    /// The log a host's save button reads; an editor calls it when its log
    /// moved.
    changed: function () {
      var count = operations().length;
      fire("onEditChange", {
        dirty: count > 0,
        operations: count,
        canUndo: ask("canUndo"),
        canRedo: ask("canRedo"),
      });
    },

    /// The envelope a host hands to `Document::edit` before saving. The
    /// version is what the library takes; a check page asserts it, because
    /// nothing else here would notice the two drifting apart.
    getOperations: function () {
      return JSON.stringify({ version: 2, ops: operations() });
    },

    /// False where no editor has an edit to take back.
    undo: function () {
      return ask("undo");
    },
    redo: function () {
      return ask("redo");
    },

    /// The host saved: every editor's log resets and undo starts over.
    committed: function () {
      tell("committed");
      odr.editing.changed();
    },
  };

  /// The name the apps and the wasm package call. Same envelope.
  odr.generateDiff = function () {
    return odr.editing.getOperations();
  };

  /// The undo chord. A form field keeps its own text undo, and a key no editor
  /// took is left to the browser.
  function chordKey(event) {
    if (!editing || event.altKey || !(event.ctrlKey || event.metaKey)) {
      return;
    }
    var target = event.target;
    if (target && /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName)) {
      return;
    }
    var chord = event.key.toLowerCase();
    if (chord !== "z" && chord !== "y") {
      return;
    }
    var taken =
      chord === "y" || event.shiftKey ? odr.editing.redo() : odr.editing.undo();
    if (taken) {
      event.stopPropagation();
      event.preventDefault();
    }
  }

  if (odr.takesKeys("shortcuts")) {
    document.addEventListener("keydown", chordKey, true);
  }
})();
