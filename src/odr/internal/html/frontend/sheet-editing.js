// The sheet's editor, attached to `odr.editing` as one editor among the
// formats: the cell overlay, the locks and the `setCell` op. The mode itself,
// the refusal channel and the log a save reads are `editing.js`.
(function () {
  "use strict";

  var table = document.querySelector(".odr-sheet");
  if (table === null) {
    return;
  }

  var odr = (window.odr = window.odr || {});

  var sheet = Number(table.getAttribute("data-odr-sheet") || 0);

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

  /// Four taps on a locked cell are one snackbar, which `odr.editing` decides.
  /// The outline answers each.
  function refuse(reason, column, row) {
    outline(odr.sheet.cellAt(column, row));
    odr.editing.refuse(reason, { sheet: sheet, column: column, row: row });
  }

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
    odr.editing.changed();
    return true;
  }

  /// An undo and a redo are the same move on the page; the log tells them
  /// apart.
  function replay(entry, value) {
    close();
    odr.sheet.showValue(entry.op.column, entry.op.row, value);
    odr.editing.changed();
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
    if (!odr.editing.isEnabled() || odr.editing.refuseAt(column, row)) {
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

  /// The open editor's own keys, which no config takes away: Escape and Enter
  /// are the only way out of an overlay.
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
  /// the keys taken here never reach the pin and the sort beneath. The undo
  /// chord is `editing.js`'s, for every format.
  function pinnedKey(event) {
    var target = event.target;
    if (
      !odr.editing.isEnabled() ||
      overlay !== null ||
      event.ctrlKey ||
      event.metaKey ||
      event.altKey ||
      (target &&
        (target.isContentEditable ||
          /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName)))
    ) {
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

  if (odr.takesKeys("navigation")) {
    document.addEventListener("keydown", pinnedKey, true);
  }

  window.addEventListener("resize", function () {
    if (overlay !== null) {
      place(odr.sheet.cellAt(editingAt.column, editingAt.row));
    }
  });

  function targetPosition(event) {
    return odr.sheet.positionOf(event.target.closest("td"));
  }

  table.addEventListener("dblclick", function (event) {
    var at = odr.editing.isEnabled() ? targetPosition(event) : null;
    if (at !== null) {
      edit(at.column, at.row, null);
    }
  });

  // A locked cell says so on the click, not on the double click.
  table.addEventListener("click", function (event) {
    var at =
      odr.editing.isEnabled() && overlay === null ? targetPosition(event) : null;
    if (at !== null && odr.editing.lockAt(at.column, at.row) !== null) {
      odr.editing.refuseAt(at.column, at.row);
    }
  });

  /// The lock on the cell at (@p column, @p row), or null where it has none.
  odr.editing.lockAt = function (column, row) {
    var cell = odr.sheet.cellAt(column, row);
    return cell === null ? null : cell.getAttribute("data-odr-lock");
  };

  /// Whether the cell at (@p column, @p row) refuses a write, which is also
  /// what tells the host.
  odr.editing.refuseAt = function (column, row) {
    if (!odr.editing.isEditable()) {
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

  /// Opens the editor over a cell, as a double click does.
  odr.editing.editAt = function (column, row) {
    return edit(column, row, null);
  };

  odr.editing.attach({
    // The overlay goes with the mode: a cell nothing can commit must not keep
    // one open over it.
    disable: close,
    operations: coalesced,
    canUndo: function () {
      return history.length > 0;
    },
    canRedo: function () {
      return undone.length > 0;
    },
    /// Takes the last write back; false where there is none.
    undo: function () {
      if (history.length === 0) {
        return false;
      }
      var entry = history.pop();
      undone.push(entry);
      replay(entry, entry.before);
      return true;
    },
    redo: function () {
      if (undone.length === 0) {
        return false;
      }
      var entry = undone.pop();
      history.push(entry);
      replay(entry, entry.op.value);
      return true;
    },
    committed: function () {
      history = [];
      undone = [];
    },
  });
})();
