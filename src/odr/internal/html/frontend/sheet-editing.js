// The sheet's editor, attached to `odr.editing`: the cell overlay, the locks,
// the `setCell` and `setCellStyle` ops. The mode itself is `editing.js`.
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

  /// The outline answers every tap; `odr.editing` drops the repeated event.
  function refuse(reason, column, row) {
    outline(odr.sheet.cellAt(column, row));
    odr.editing.refuse(reason, { sheet: sheet, column: column, row: row });
  }

  var overlay = null;
  var editingAt = null;
  var history = [];
  var undone = [];

  /// One op per kind and position: the last value written, and the style
  /// keys merged with the later ones winning.
  function coalesced() {
    var byKey = new Map();
    for (var i = 0; i < history.length; ++i) {
      var ops = history[i].ops;
      for (var j = 0; j < ops.length; ++j) {
        var op = ops[j];
        var key = op.op + ":" + op.sheet + ":" + op.column + ":" + op.row;
        var earlier = byKey.get(key);
        if (op.op === "setCellStyle" && earlier !== undefined) {
          op = {
            op: op.op,
            sheet: op.sheet,
            column: op.column,
            row: op.row,
            style: Object.assign({}, earlier.style, op.style),
          };
        }
        byKey.set(key, op);
      }
    }
    return Array.from(byKey.values());
  }

  /// An undo and a redo are the same move on the page; the log tells them
  /// apart.
  function replay(move) {
    close();
    move();
    repaintStale();
    odr.editing.changed();
    reportSelection(true);
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
      ops: [
        { op: "setCell", sheet: sheet, column: column, row: row, value: value },
      ],
      undo: function () {
        odr.sheet.showValue(column, row, before);
      },
      redo: function () {
        odr.sheet.showValue(column, row, value);
      },
    });
    undone = [];
    repaintStale();
    odr.editing.changed();
    return true;
  }

  /// What each formula cell reads, off `data-odr-reads`. Only the rectangles
  /// in this sheet, because an edit here names a position in it.
  var readers = null;

  function bound(text) {
    return text === "*" ? Infinity : Number(text);
  }

  function readersOf() {
    if (readers !== null) {
      return readers;
    }
    readers = [];
    var cells = table.querySelectorAll("td[data-odr-reads]");
    for (var i = 0; i < cells.length; ++i) {
      var at = odr.sheet.positionOf(cells[i]);
      if (at === null) {
        continue;
      }
      var boxes = [];
      var groups = cells[i].getAttribute("data-odr-reads").split(" ");
      for (var j = 0; j < groups.length; ++j) {
        var parts = groups[j].split(",");
        if (parts.length === 5 && Number(parts[0]) === sheet) {
          boxes.push([
            bound(parts[1]),
            bound(parts[2]),
            bound(parts[3]),
            bound(parts[4]),
          ]);
        }
      }
      if (boxes.length > 0) {
        readers.push({ cell: cells[i], at: at, reads: boxes });
      }
    }
    return readers;
  }

  function readsAny(entry, positions) {
    for (var i = 0; i < entry.reads.length; ++i) {
      var box = entry.reads[i];
      for (var j = 0; j < positions.length; ++j) {
        var at = positions[j];
        if (
          at.column >= box[0] &&
          at.column <= box[1] &&
          at.row >= box[2] &&
          at.row <= box[3]
        ) {
          return true;
        }
      }
    }
    return false;
  }

  /// Every cell reading one of @p positions, and every cell reading one of
  /// those: a formula whose input went stale is stale itself.
  function staleFrom(positions) {
    var entries = readersOf();
    var taken = [];
    var frontier = positions;
    var result = [];
    while (frontier.length > 0) {
      var next = [];
      for (var i = 0; i < entries.length; ++i) {
        if (taken[i] || !readsAny(entries[i], frontier)) {
          continue;
        }
        taken[i] = true;
        result.push(entries[i]);
        next.push(entries[i].at);
      }
      frontier = next;
    }
    return result;
  }

  var stale = [];
  var staleKey = "";

  /// The marks follow the log, not the last write, so an undo takes back what
  /// it made stale and a save clears them with the log.
  function repaintStale() {
    var written = [];
    var ops = coalesced();
    for (var i = 0; i < ops.length; ++i) {
      if (ops[i].op === "setCell") {
        written.push({ column: ops[i].column, row: ops[i].row });
      }
    }

    for (var j = 0; j < stale.length; ++j) {
      stale[j].classList.remove("odr-sheet-stale");
    }
    stale = [];

    var cells = [];
    var entries = staleFrom(written);
    for (var k = 0; k < entries.length; ++k) {
      entries[k].cell.classList.add("odr-sheet-stale");
      stale.push(entries[k].cell);
      cells.push({ column: entries[k].at.column, row: entries[k].at.row });
    }

    // A host hears when the set moved, not on every keystroke.
    var key = cells
      .map(function (at) {
        return at.column + ":" + at.row;
      })
      .join(" ");
    if (key !== staleKey) {
      staleKey = key;
      odr.editing.stale({ sheet: sheet, cells: cells });
    }
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

  /// The open editor's own keys, which no config takes away: they are the way
  /// out of the overlay.
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
  /// these never reach the pin and the sort beneath. The chord is `editing.js`'s.
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
    if (event.shiftKey && arrows[event.key] !== undefined) {
      var to = odr.sheet.selection().focus;
      odr.sheet.select({ column: to.column + step[0], row: to.row + step[1] });
    } else if (step !== null) {
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

  /// Gesture policy, the viewer's to set. `editOnClick` unstated asks the
  /// pointer.
  var options = { editOnClick: null };

  /// Whether a click opens the editor, where a double click always does. The
  /// pointer answers only while `editOnClick` is unstated, because it is a
  /// guess: an android WebView reports a fine one on a touch screen.
  function tapEdits() {
    // `!= null` so an unset key a host passes reads as unstated, not as off
    if (options.editOnClick != null) {
      return !!options.editOnClick;
    }
    return (
      typeof window.matchMedia === "function" &&
      window.matchMedia("(pointer: coarse)").matches
    );
  }

  /// Merged into what is set; an unknown key throws.
  odr.editing.setSheetOptions = function (value) {
    Object.keys(value || {}).forEach(function (key) {
      if (!Object.prototype.hasOwnProperty.call(options, key)) {
        throw new Error("odr.editing: unknown sheet option " + key);
      }
      options[key] = value[key];
    });
  };

  odr.editing.getSheetOptions = function () {
    var copy = {};
    Object.keys(options).forEach(function (key) {
      copy[key] = options[key];
    });
    return copy;
  };

  // A locked cell says so on the click, not on the double click. A shift
  // click spans a selection and opens nothing.
  table.addEventListener("click", function (event) {
    var at =
      odr.editing.isEnabled() && overlay === null && !event.shiftKey
        ? targetPosition(event)
        : null;
    if (at === null) {
      return;
    }
    if (odr.editing.lockAt(at.column, at.row) !== null) {
      odr.editing.refuseAt(at.column, at.row);
      return;
    }
    if (tapEdits()) {
      edit(at.column, at.row, null);
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

  // ------------------------------------------------------------- formatting

  var TOGGLES = ["bold", "italic", "underline", "strikethrough"];
  var KEYS = TOGGLES.concat(["color", "size", "fill", "align"]);

  /// `#rrggbb` for a computed `rgb(…)`, null for a transparent one.
  function hexOf(computed) {
    var match = /^rgba?\((\d+),\s*(\d+),\s*(\d+)(?:,\s*([\d.]+))?\)$/.exec(
      computed
    );
    if (match === null || (match[4] !== undefined && Number(match[4]) === 0)) {
      return null;
    }
    var hex = "#";
    for (var i = 1; i <= 3; ++i) {
      hex += ("0" + Number(match[i]).toString(16)).slice(-2);
    }
    return hex;
  }

  // `html::dark_fill`: the lightness mirrored in oklab into the band from
  // the dark page to the lightest ground its text reads on, the hue kept.
  function toLinear(c) {
    c /= 255;
    return c <= 0.04045 ? c / 12.92 : Math.pow((c + 0.055) / 1.055, 2.4);
  }

  function toOklab(r, g, b) {
    r = toLinear(r);
    g = toLinear(g);
    b = toLinear(b);
    var l = Math.cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    var m = Math.cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    var s = Math.cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return [
      0.2104542553 * l + 0.793617785 * m - 0.0040720468 * s,
      1.9779984951 * l - 2.428592205 * m + 0.4505937099 * s,
      0.0259040371 * l + 0.7827717662 * m - 0.808675766 * s,
    ];
  }

  function toLinearRgb(lab) {
    var l = Math.pow(lab[0] + 0.3963377774 * lab[1] + 0.2158037573 * lab[2], 3);
    var m = Math.pow(lab[0] - 0.1055613458 * lab[1] - 0.0638541728 * lab[2], 3);
    var s = Math.pow(lab[0] - 0.0894841775 * lab[1] - 1.291485548 * lab[2], 3);
    return [
      4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
      -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
      -0.0041960863 * l - 0.7034186147 * m + 1.707614701 * s,
    ];
  }

  function inGamut(rgb) {
    return rgb.every(function (c) {
      return c >= -1e-9 && c <= 1 + 1e-9;
    });
  }

  var PAGE = toOklab(0x16, 0x1b, 0x22);
  var LIGHTEST = 0.5;

  function darkFill(hex) {
    var source = toOklab(
      parseInt(hex.slice(1, 3), 16),
      parseInt(hex.slice(3, 5), 16),
      parseInt(hex.slice(5, 7), 16)
    );
    var t = 1 - source[0];
    var lab = [
      PAGE[0] + t * (LIGHTEST - PAGE[0]),
      PAGE[1] * (1 - t) + source[1],
      PAGE[2] * (1 - t) + source[2],
    ];
    if (!inGamut(toLinearRgb(lab))) {
      var low = 0;
      var high = 1;
      for (var i = 0; i < 20; ++i) {
        var mid = (low + high) / 2;
        if (inGamut(toLinearRgb([lab[0], lab[1] * mid, lab[2] * mid]))) {
          low = mid;
        } else {
          high = mid;
        }
      }
      lab = [lab[0], lab[1] * low, lab[2] * low];
    }
    return (
      "#" +
      toLinearRgb(lab)
        .map(function (c) {
          c = c <= 0.0031308 ? c * 12.92 : 1.055 * Math.pow(c, 1 / 2.4) - 0.055;
          var byte = Math.round(Math.min(Math.max(c, 0), 1) * 255);
          return ("0" + byte.toString(16)).slice(-2);
        })
        .join("")
    );
  }

  /// The blocks a cell's text sits in, the cell itself where it writes its
  /// string straight in.
  function holdersOf(cell) {
    var blocks = cell.querySelectorAll(":scope>x-p");
    return blocks.length > 0 ? Array.prototype.slice.call(blocks) : [cell];
  }

  /// The style the cell shows, as the wire spells it; computed, so what the
  /// hoisted classes give it counts.
  function styleOf(cell) {
    var text = cell.querySelector("x-s") || cell.querySelector("x-p") || cell;
    var computed = getComputedStyle(text);
    var lines = "";
    for (var node = text; node !== null; node = node.parentElement) {
      lines += " " + getComputedStyle(node).textDecorationLine;
      if (node === cell) {
        break;
      }
    }
    var weight = computed.fontWeight;
    var px = parseFloat(computed.fontSize);
    // the dark sheet paints `--odr-dark-fill`, and `--odr-fill` keeps the
    // colour it stands for
    var cellStyle = getComputedStyle(cell);
    var stated = cellStyle.getPropertyValue("--odr-fill").trim();
    var align = getComputedStyle(holdersOf(cell)[0]).textAlign;
    return {
      bold: weight === "bold" || Number(weight) >= 600,
      italic: computed.fontStyle === "italic" || computed.fontStyle === "oblique",
      underline: lines.indexOf("underline") !== -1,
      strikethrough: lines.indexOf("line-through") !== -1,
      color: hexOf(computed.color),
      size: isNaN(px) ? null : String(Math.round(px * 75) / 100) + "pt",
      fill:
        stated === ""
          ? hexOf(cellStyle.backgroundColor)
          : /^#[0-9a-f]{6}$/i.test(stated)
            ? stated.toLowerCase()
            : null,
      align:
        align === "center"
          ? "center"
          : align === "right" || align === "end"
            ? "right"
            : align === "left" || align === "start"
              ? "left"
              : null,
    };
  }

  /// What the selected cells agree on: a key per property with one value
  /// across them, and none where they differ.
  function summary() {
    var result = {};
    var cells = odr.sheet.selectedCells();
    if (cells.length === 0) {
      return result;
    }
    var styles = cells.map(function (at) {
      return styleOf(at.cell);
    });
    KEYS.forEach(function (key) {
      for (var i = 1; i < styles.length; ++i) {
        if (styles[i][key] !== styles[0][key]) {
          return;
        }
      }
      result[key] = styles[0][key];
    });
    return result;
  }

  var lastReported = null;

  function reportSelection(force) {
    if (!odr.editing.isEnabled()) {
      return;
    }
    var shown = summary();
    var key = JSON.stringify(shown);
    if (key === lastReported && force !== true) {
      return;
    }
    lastReported = key;
    odr.editing.selectionChanged(shown);
  }

  table.addEventListener("odr-sheet-select", function () {
    reportSelection(false);
  });

  /// The `style` attribute of the cell and of each element in it, which is
  /// all a format writes.
  function snapshot(cell) {
    return [cell]
      .concat(Array.prototype.slice.call(cell.querySelectorAll("x-p,x-s")))
      .map(function (node) {
        return { node: node, style: node.getAttribute("style") };
      });
  }

  function restore(shot) {
    for (var i = 0; i < shot.length; ++i) {
      if (shot[i].style === null) {
        shot[i].node.removeAttribute("style");
      } else {
        shot[i].node.setAttribute("style", shot[i].style);
      }
    }
  }

  /// Writes @p style onto the cell as the renderer would: the cell and every
  /// run in it take the text keys, the blocks take the lines and the
  /// alignment, and a fill carries the colour the dark sheet paints.
  function paintCell(cell, style) {
    var texts = [cell].concat(
      Array.prototype.slice.call(cell.querySelectorAll("x-p,x-s"))
    );
    var runs = cell.querySelectorAll("x-s");
    var holders = holdersOf(cell);
    var shown = styleOf(cell);
    texts.forEach(function (node) {
      if (style.bold !== undefined) {
        node.style.fontWeight = style.bold ? "bold" : "normal";
      }
      if (style.italic !== undefined) {
        node.style.fontStyle = style.italic ? "italic" : "normal";
      }
      if (style.color !== undefined) {
        node.style.color = style.color;
      }
      if (style.size !== undefined) {
        node.style.fontSize = style.size;
      }
    });
    if (style.underline !== undefined || style.strikethrough !== undefined) {
      var lines = [];
      if (style.underline !== undefined ? style.underline : shown.underline) {
        lines.push("underline");
      }
      if (
        style.strikethrough !== undefined
          ? style.strikethrough
          : shown.strikethrough
      ) {
        lines.push("line-through");
      }
      holders.forEach(function (node) {
        node.style.textDecorationLine =
          lines.length === 0 ? "none" : lines.join(" ");
      });
      for (var i = 0; i < runs.length; ++i) {
        runs[i].style.textDecorationLine = "none";
      }
    }
    if (style.align !== undefined) {
      holders.concat(holders[0] === cell ? [] : [cell]).forEach(function (node) {
        node.style.textAlign = style.align;
      });
    }
    if (style.fill !== undefined) {
      var fill = style.fill === null ? "transparent" : style.fill;
      cell.style.backgroundColor = fill;
      cell.style.setProperty("--odr-fill", fill);
      cell.style.setProperty(
        "--odr-dark-fill",
        style.fill === null ? "transparent" : darkFill(style.fill)
      );
    }
  }

  var COLOR = /^#[0-9a-f]{6}$/i;
  var SIZE = /^[0-9]*\.?[0-9]+(pt|px|in|cm|mm|pc)$/;

  /// Whether @p style is one the op can carry.
  function valid(style) {
    return Object.keys(style).every(function (key) {
      var value = style[key];
      if (TOGGLES.indexOf(key) !== -1) {
        return typeof value === "boolean";
      }
      if (key === "color") {
        return COLOR.test(value);
      }
      if (key === "fill") {
        return value === null || COLOR.test(value);
      }
      if (key === "size") {
        return SIZE.test(value);
      }
      if (key === "align") {
        return value === "left" || value === "center" || value === "right";
      }
      return false;
    });
  }

  /// States @p style on every selected cell as one undo step. A lock refuses
  /// a value, not a style.
  function format(style) {
    finish();
    if (!odr.editing.isEnabled()) {
      return false;
    }
    if (!odr.editing.isEditable()) {
      odr.editing.refuse("readOnly", { sheet: sheet });
      return false;
    }
    var cells = odr.sheet.selectedCells();
    if (cells.length === 0 || !valid(style)) {
      odr.editing.refuse("unsupportedEdit", { sheet: sheet });
      return false;
    }
    var befores = [];
    var afters = [];
    var ops = [];
    var rows = new Set();
    cells.forEach(function (at) {
      befores.push(snapshot(at.cell));
      paintCell(at.cell, style);
      afters.push(snapshot(at.cell));
      ops.push({
        op: "setCellStyle",
        sheet: sheet,
        column: at.column,
        row: at.row,
        style: Object.assign({}, style),
      });
      rows.add(at.row);
    });
    var reflow = function () {
      rows.forEach(function (row) {
        odr.sheet.reflow(row);
      });
    };
    history.push({
      ops: ops,
      undo: function () {
        befores.forEach(restore);
        reflow();
      },
      redo: function () {
        afters.forEach(restore);
        reflow();
      },
    });
    undone = [];
    reflow();
    odr.editing.changed();
    reportSelection(true);
    return true;
  }

  /// A mixed selection turns on, as Word does.
  function toggle(property) {
    if (TOGGLES.indexOf(property) === -1) {
      odr.editing.refuse("unsupportedEdit", { sheet: sheet });
      return false;
    }
    var style = {};
    style[property] = summary()[property] !== true;
    return format(style);
  }

  var chords = { b: "bold", i: "italic", u: "underline" };

  /// The formatting chords, where the config gives the scripts the shortcuts.
  function chordKey(event) {
    if (
      !odr.editing.isEnabled() ||
      overlay !== null ||
      event.altKey ||
      !(event.ctrlKey || event.metaKey)
    ) {
      return;
    }
    var property = chords[event.key.toLowerCase()];
    if (property === undefined || odr.sheet.selectedCells().length === 0) {
      return;
    }
    toggle(property);
    event.stopPropagation();
    event.preventDefault();
  }

  if (odr.takesKeys("shortcuts")) {
    document.addEventListener("keydown", chordKey, true);
  }

  odr.editing.attach({
    // A cell nothing can commit must keep no overlay open over it.
    disable: close,
    operations: coalesced,
    canUndo: function () {
      return history.length > 0;
    },
    canRedo: function () {
      return undone.length > 0;
    },
    /// Takes the last step back; false where there is none.
    undo: function () {
      if (history.length === 0) {
        return false;
      }
      var entry = history.pop();
      undone.push(entry);
      replay(entry.undo);
      return true;
    },
    redo: function () {
      if (undone.length === 0) {
        return false;
      }
      var entry = undone.pop();
      history.push(entry);
      replay(entry.redo);
      return true;
    },
    format: format,
    toggle: toggle,
    enable: function () {
      reportSelection(true);
    },
    committed: function () {
      history = [];
      undone = [];
      repaintStale();
    },
  });
})();
