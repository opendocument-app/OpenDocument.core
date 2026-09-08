(function () {
  "use strict";

  var table = document.querySelector(".odr-sheet");
  if (table === null) {
    return;
  }

  var odr = (window.odr = window.odr || {});

  var sheet = Number(table.getAttribute("data-odr-sheet") || 0);
  var editable = table.getAttribute("data-odr-editable") === "true";
  var editing = false;
  var lastRefusal = null;

  // One space with `odr.onError`'s codes, appended and never renumbered - 1 is
  // `errorIllegalEditNewLine`. The host maps the code to its own wording; the
  // message is for a developer who wires nothing.
  var refusals = {
    formula: { code: 2, message: "cell holds a formula" },
    rich: { code: 3, message: "cell holds more than one plain run" },
    shapes: { code: 4, message: "cell holds a drawing" },
    readOnly: { code: 5, message: "document cannot be edited" },
    formulaInput: { code: 6, message: "typing a formula is not supported" },
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

  var outlined = null;
  var outlinedTimer = 0;

  /// The outline a refusal paints, so a host wiring nothing is not silent.
  function outline(cell) {
    if (outlined !== null) {
      outlined.classList.remove("odr-sheet-refused");
    }
    window.clearTimeout(outlinedTimer);
    outlined = cell;
    if (cell === null) {
      return;
    }
    cell.classList.add("odr-sheet-refused");
    outlinedTimer = window.setTimeout(function () {
      cell.classList.remove("odr-sheet-refused");
      outlined = null;
    }, 700);
  }

  /// Four taps on a locked cell are one snackbar: the same refusal within two
  /// seconds of the last is the page's to drop. The outline answers each.
  function refuse(reason, column, row) {
    var refusal = refusals[reason] || refusals.readOnly;
    var key = reason + ":" + column + ":" + row;
    var now = Date.now();
    outline(odr.sheet.cellAt(column, row));
    if (lastRefusal && lastRefusal.key === key && now - lastRefusal.at < 2000) {
      return;
    }
    lastRefusal = { key: key, at: now };
    fire("onEditRefused", {
      sheet: sheet,
      column: column,
      row: row,
      reason: reason,
      code: refusal.code,
      message: refusal.message,
    });
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
    /// Answers whether the mode is on. A document that cannot be edited
    /// refuses and says why, so a host can grey its button before a click.
    enable: function () {
      if (!editable) {
        modeChange("readOnly");
        return false;
      }
      if (!editing) {
        editing = true;
        table.classList.add("odr-editing");
        modeChange(null);
      }
      return true;
    },
    disable: function () {
      if (editing) {
        editing = false;
        close();
        table.classList.remove("odr-editing");
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
    /// The lock on the cell at (@p column, @p row), or null where it has none.
    lockAt: function (column, row) {
      var cell = odr.sheet.cellAt(column, row);
      return cell === null ? null : cell.getAttribute("data-odr-lock");
    },
  };

  /// Whether the cell at (@p column, @p row) refuses a write, which is also
  /// what tells the host.
  odr.editing.refuseAt = function (column, row) {
    if (!editable) {
      refuse("readOnly", column, row);
      return true;
    }
    var lock = odr.editing.lockAt(column, row);
    if (lock !== null) {
      refuse(lock, column, row);
      return true;
    }
    return false;
  };

  var overlay = null;
  var editingAt = null;
  var history = [];
  var undone = [];

  /// One op per position, the last write made.
  function coalesced() {
    var byPosition = new Map();
    for (var i = 0; i < history.length; ++i) {
      var op = history[i].op;
      byPosition.set(op.sheet + ":" + op.column + ":" + op.row, op);
    }
    return Array.from(byPosition.values());
  }

  /// What a host's save button and back-press warning read.
  function changed() {
    fire("onEditChange", {
      dirty: history.length > 0,
      operations: coalesced().length,
      canUndo: history.length > 0,
      canRedo: undone.length > 0,
    });
  }

  var NUMBER = /^[+-]?([0-9]+(\.[0-9]*)?|\.[0-9]+)([eE][+-]?[0-9]+)?$/;

  /// The type follows the string the user typed: a number where the grammar
  /// says so, a string otherwise, and a leading `'` forces one.
  function parse(text) {
    var quoted = text.charAt(0) === "'";
    var content = quoted ? text.slice(1) : text;
    if (content === "") {
      return { type: "empty" };
    }
    if (!quoted && NUMBER.test(content)) {
      return { type: "number", number: Number(content), text: content };
    }
    return { type: "string", text: content };
  }

  function same(one, other) {
    return (
      one.type === other.type &&
      (one.type === "empty" || one.text === other.text)
    );
  }

  /// Writes @p value at a position: the cell shows it, and the op joins the
  /// log beside the value it replaced.
  function write(column, row, value) {
    var before = odr.sheet.valueAt(column, row);
    if (before === null || same(before, value)) {
      return false;
    }
    if (!odr.sheet.showValue(column, row, value)) {
      return false;
    }
    history.push({
      op: {
        op: "setCell",
        sheet: sheet,
        column: column,
        row: row,
        value: value,
      },
      before: before,
    });
    undone = [];
    changed();
    return true;
  }

  /// An undo and a redo are the same move on the page; the log tells them
  /// apart.
  function replay(entry, value) {
    close();
    odr.sheet.showValue(entry.op.column, entry.op.row, value);
    changed();
  }

  // Offsets, not rects: blink scales a rect by the body zoom `viewport_js`
  // applies, and the overlay is laid out under that zoom.
  function place(cell) {
    var left = 0;
    var top = 0;
    for (var node = cell; node !== null; node = node.offsetParent) {
      left += node.offsetLeft;
      top += node.offsetTop;
    }
    var style = getComputedStyle(cell);
    overlay.style.left = left + "px";
    overlay.style.top = top + "px";
    overlay.style.width = cell.offsetWidth + "px";
    overlay.style.height = cell.offsetHeight + "px";
    overlay.style.textAlign = style.textAlign;
    overlay.style.color = style.color;
    overlay.style.fontFamily = style.fontFamily;
    overlay.style.fontSize = style.fontSize;
    overlay.style.fontStyle = style.fontStyle;
    overlay.style.fontWeight = style.fontWeight;
  }

  /// Opens the editor over a cell, holding @p typed or the cell's own string.
  /// The raise is put back down: the overlay shows what it would have.
  function edit(column, row, typed) {
    finish();
    if (!editing || odr.editing.refuseAt(column, row)) {
      return false;
    }
    var cell = odr.sheet.cellAt(column, row);
    if (cell === null) {
      return false;
    }
    odr.sheet.pin({ column: column, row: row });
    odr.sheet.lower();

    var value = odr.sheet.valueAt(column, row);
    editingAt = { column: column, row: row };
    overlay = document.createElement("input");
    overlay.type = "text";
    overlay.className = "odr-sheet-editor";
    overlay.value =
      typed !== null ? typed : value.type === "empty" ? "" : value.text;
    place(cell);
    document.body.appendChild(overlay);
    overlay.addEventListener("keydown", overlayKey);
    overlay.addEventListener("blur", finish);
    overlay.focus();
    if (typed === null) {
      overlay.select();
    }
    return true;
  }

  function close() {
    var input = overlay;
    overlay = null;
    editingAt = null;
    if (input !== null) {
      input.remove();
    }
  }

  /// Ends an open edit: what it holds is committed, and a refused formula is
  /// dropped rather than left in an overlay nothing focuses again.
  function finish() {
    if (!commit(0, 0)) {
      close();
    }
  }

  /// Commits what is typed and moves the pin by (@p columns, @p rows). A
  /// formula is refused rather than written, and leaves the editor open.
  function commit(columns, rows) {
    if (overlay === null) {
      return false;
    }
    var text = overlay.value;
    var at = editingAt;
    if (text.charAt(0) === "=") {
      refuse("formulaInput", at.column, at.row);
      return false;
    }
    close();
    write(at.column, at.row, parse(text));
    if (!odr.sheet.pin({ column: at.column + columns, row: at.row + rows })) {
      odr.sheet.pin({ column: at.column, row: at.row });
    }
    return true;
  }

  function overlayKey(event) {
    // Typing is the overlay's, not the sheet's underneath it.
    event.stopPropagation();
    if (event.key === "Escape") {
      close();
    } else if (event.key === "Enter") {
      commit(0, event.shiftKey ? -1 : 1);
    } else if (event.key === "Tab") {
      commit(event.shiftKey ? -1 : 1, 0);
    } else {
      return;
    }
    event.preventDefault();
  }

  var arrows = {
    ArrowUp: [0, -1],
    ArrowDown: [0, 1],
    ArrowLeft: [-1, 0],
    ArrowRight: [1, 0],
  };

  /// What a pinned cell does with a key when no editor is open. Captured, so
  /// the keys taken here never reach the pin and the sort beneath.
  function pinnedKey(event) {
    var target = event.target;
    if (
      !editing ||
      overlay !== null ||
      (target &&
        (target.isContentEditable ||
          /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName)))
    ) {
      return;
    }

    // ctrl/cmd is the undo chord here and nothing else.
    if (event.ctrlKey || event.metaKey || event.altKey) {
      var chord = event.key.toLowerCase();
      if (!event.altKey && (chord === "z" || chord === "y")) {
        if (chord === "y" || event.shiftKey) {
          odr.editing.redo();
        } else {
          odr.editing.undo();
        }
        event.stopPropagation();
        event.preventDefault();
      }
      return;
    }

    var at = odr.sheet.pinned();
    if (at === null || at.column === null || at.row === null) {
      return;
    }

    var step =
      arrows[event.key] ||
      (event.key === "Tab" ? [event.shiftKey ? -1 : 1, 0] : null);
    if (step !== null) {
      odr.sheet.pin({ column: at.column + step[0], row: at.row + step[1] });
    } else if (event.key === "Enter" || event.key === "F2") {
      edit(at.column, at.row, null);
    } else if (event.key === "Delete" || event.key === "Backspace") {
      if (!odr.editing.refuseAt(at.column, at.row)) {
        write(at.column, at.row, { type: "empty" });
      }
    } else if (event.key.length === 1) {
      edit(at.column, at.row, event.key);
    } else {
      return;
    }
    event.stopPropagation();
    event.preventDefault();
  }

  document.addEventListener("keydown", pinnedKey, true);

  window.addEventListener("resize", function () {
    if (overlay !== null) {
      place(odr.sheet.cellAt(editingAt.column, editingAt.row));
    }
  });

  function targetPosition(event) {
    return odr.sheet.positionOf(event.target.closest("td"));
  }

  table.addEventListener("dblclick", function (event) {
    var at = editing ? targetPosition(event) : null;
    if (at !== null) {
      edit(at.column, at.row, null);
    }
  });

  // A locked cell says so on the click, not on the double click.
  table.addEventListener("click", function (event) {
    var at = editing && overlay === null ? targetPosition(event) : null;
    if (at !== null && odr.editing.lockAt(at.column, at.row) !== null) {
      odr.editing.refuseAt(at.column, at.row);
    }
  });

  /// Opens the editor over a cell, as a double click does.
  odr.editing.editAt = function (column, row) {
    return edit(column, row, null);
  };

  /// The envelope a host hands to `Document::edit` before saving.
  odr.editing.getOperations = function () {
    return JSON.stringify({ version: 1, ops: coalesced() });
  };

  /// Takes the last write back; false where there is none.
  odr.editing.undo = function () {
    if (history.length === 0) {
      return false;
    }
    var entry = history.pop();
    undone.push(entry);
    replay(entry, entry.before);
    return true;
  };

  odr.editing.redo = function () {
    if (undone.length === 0) {
      return false;
    }
    var entry = undone.pop();
    history.push(entry);
    replay(entry, entry.op.value);
    return true;
  };

  /// The host saved the log: the page and the file agree, and undo starts
  /// over.
  odr.editing.committed = function () {
    history = [];
    undone = [];
    changed();
  };
})();
