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

  // One space of codes, appended and never renumbered. The host maps the code;
  // the message is for a console.
  var refusals = {
    newLine: { code: 1, message: "new line not supported by this document" },
    formula: { code: 2, message: "cell holds a formula" },
    rich: { code: 3, message: "cell holds more than one plain run" },
    shapes: { code: 4, message: "cell holds a drawing" },
    readOnly: { code: 5, message: "document cannot be edited" },
    formulaInput: { code: 6, message: "typing a formula is not supported" },
  };

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

  function modeChange(reason) {
    fire("onEditModeChange", {
      editing: editing,
      editable: editable,
      reason: reason || null,
      code: reason ? refusals[reason].code : 0,
      message: reason ? refusals[reason].message : "",
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

    /// Adds one format's editor. Only `operations` is required; `enable`,
    /// `disable`, `undo`, `redo`, `canUndo`, `canRedo` and `committed` default.
    attach: function (editor) {
      editors.push(editor);
    },

    /// Reports a refused edit, dropping a repeat of the same one within two
    /// seconds: four taps on a locked cell are one snackbar. @p detail is how
    /// the format addresses it. Painting it is the editor's.
    refuse: function (reason, detail) {
      var refusal = refusals[reason] || refusals.readOnly;
      var key = reason + ":" + JSON.stringify(detail || null);
      var now = Date.now();
      if (lastRefusal !== null && lastRefusal.key === key && now - lastRefusal.at < 2000) {
        return;
      }
      lastRefusal = { key: key, at: now };
      var event = { reason: reason, code: refusal.code, message: refusal.message };
      for (var field in detail) {
        if (Object.prototype.hasOwnProperty.call(detail, field)) {
          event[field] = detail[field];
        }
      }
      fire("onEditRefused", event);
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

    /// The envelope a host hands to `Document::edit` before saving.
    getOperations: function () {
      return JSON.stringify({ version: 1, ops: operations() });
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
