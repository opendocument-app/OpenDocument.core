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
  // The extent of the whole sheet, stated only where the page renders less.
  var cut = (function () {
    var stated = table.getAttribute("data-odr-cut");
    if (stated === null) {
      return null;
    }
    var extent = stated.split(",").map(Number);
    return { columns: extent[0], rows: extent[1] };
  })();

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

  /// Refuses a move to a position the sheet has and the page does not.
  function refuseCut(column, row) {
    if (
      cut !== null &&
      column >= 0 &&
      row >= 0 &&
      column < cut.columns &&
      row < cut.rows &&
      odr.sheet.cellAt(column, row) === null
    ) {
      odr.editing.refuse("sheetCut", {
        sheet: sheet,
        column: column,
        row: row,
        columns: cut.columns,
        rows: cut.rows,
      });
    }
  }

  var overlay = null;
  var editingAt = null;
  var history = [];
  var undone = [];

  var STYLE_OPS = ["setCellStyle", "setRowStyle", "setColumnStyle"];
  /// The axis a row or column op moves along.
  var STRUCTURE_OPS = {
    insertRows: "row",
    deleteRows: "row",
    insertColumns: "column",
    deleteColumns: "column",
  };

  /// Whether two style ops reach a cell in common. An absent axis is a whole
  /// row or column.
  function overlaps(one, other) {
    return (
      (one.column === undefined ||
        other.column === undefined ||
        one.column === other.column) &&
      (one.row === undefined || other.row === undefined || one.row === other.row)
    );
  }

  /// Coalesces values and style keys by position, with later writes winning.
  /// Overlapping style operations and structural edits are merge barriers.
  function coalesced() {
    var result = [];
    var byKey = new Map();
    for (var i = 0; i < history.length; ++i) {
      var ops = history[i].ops;
      for (var j = 0; j < ops.length; ++j) {
        var op = ops[j];
        if (STRUCTURE_OPS[op.op] !== undefined) {
          byKey.clear();
          result.push(op);
          continue;
        }
        var key = op.op + ":" + op.sheet + ":" + op.column + ":" + op.row;
        var at = byKey.get(key);
        var styles = STYLE_OPS.indexOf(op.op) !== -1;
        if (at === undefined) {
          byKey.set(key, result.length);
          result.push(op);
        } else if (styles) {
          result[at] = Object.assign({}, op, {
            style: Object.assign({}, result[at].style, op.style),
          });
        } else {
          result[at] = op;
        }
        if (styles) {
          byKey.forEach(function (index, other) {
            var earlier = result[index];
            if (
              other !== key &&
              STYLE_OPS.indexOf(earlier.op) !== -1 &&
              overlaps(earlier, op)
            ) {
              byKey.delete(other);
            }
          });
        }
      }
    }
    return result;
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

  /// The document's decimal separator, defaulting to `.`.
  var DECIMAL = (function () {
    var locale = document.body.getAttribute("data-odr-locale");
    try {
      var parts = new Intl.NumberFormat(locale || "en").formatToParts(1.5);
      for (var i = 0; i < parts.length; ++i) {
        if (parts[i].type === "decimal") {
          return parts[i].value;
        }
      }
    } catch (e) {
      // a tag the browser does not know
    }
    return ".";
  })();
  var NUMBER = new RegExp(
    "^[+-]?([0-9]+(D[0-9]*)?|D[0-9]+)([eE][+-]?[0-9]+)?$".replace(
      /D/g,
      DECIMAL.replace(/[.*+?^${}()|[\]\\]/g, "\\$&")
    )
  );

  // A date is typed in the document's order, else the reader's: a file that
  // states no locale has no order of its own. Gregorian and Latin digits, so
  // what `spell` writes `parse` reads back.
  var DATE_FORMAT = (function () {
    var options = {
      year: "numeric",
      month: "numeric",
      day: "numeric",
      timeZone: "UTC",
      calendar: "gregory",
      numberingSystem: "latn",
    };
    var locale =
      document.body.getAttribute("data-odr-locale") ||
      navigator.language ||
      "en-US";
    try {
      return new Intl.DateTimeFormat(locale, options);
    } catch (e) {
      return new Intl.DateTimeFormat("en-US", options);
    }
  })();

  /// The order of day, month and year, as `dmy`, and the sign between them.
  var DATE_ORDER = (function () {
    var order = "";
    var separator = null;
    DATE_FORMAT.formatToParts(Date.UTC(2025, 10, 22)).forEach(function (part) {
      if (part.type === "day" || part.type === "month" || part.type === "year") {
        order += part.type.charAt(0);
      } else if (part.type === "literal" && separator === null) {
        separator = part.value.trim() || part.value;
      }
    });
    return order.length === 3 && separator !== null
      ? { order: order, separator: separator }
      : { order: "mdy", separator: "/" };
  })();

  function escaped(text) {
    return text.replace(/[.*+?^${}()|[\]\\]/g, "\\$&");
  }

  var ISO_DATE = /^(\d{4})-(\d{1,2})-(\d{1,2})$/;
  var LOCAL_DATE = new RegExp(
    "^(\\d{1,4})\\s*S\\s*(\\d{1,2})\\s*S\\s*(\\d{1,4})\\s*S?$".replace(
      /S/g,
      escaped(DATE_ORDER.separator)
    )
  );
  var TIME =
    /^(-?)(\d{1,4}):(\d{2})(?::(\d{2})(?:[.,](\d{1,3}))?)?(?:\s*([ap])\.?m?\.?)?$/i;

  /// Days since 1899-12-30, or null where the date is none.
  function civilDays(year, month, day) {
    var date = new Date(0);
    date.setUTCFullYear(year, month - 1, day);
    if (
      date.getUTCFullYear() !== year ||
      date.getUTCMonth() !== month - 1 ||
      date.getUTCDate() !== day
    ) {
      return null;
    }
    return date.getTime() / 86400000 + 25569;
  }

  /// A typed date as days since 1899-12-30; a two-digit year below 30 is 20xx,
  /// as Excel reads one.
  function readDate(text) {
    var iso = ISO_DATE.exec(text);
    if (iso !== null) {
      return civilDays(Number(iso[1]), Number(iso[2]), Number(iso[3]));
    }
    var local = LOCAL_DATE.exec(text);
    if (local === null) {
      return null;
    }
    var parts = {};
    for (var i = 0; i < 3; ++i) {
      parts[DATE_ORDER.order.charAt(i)] = local[i + 1];
    }
    var year = Number(parts.y);
    if (parts.y.length <= 2) {
      year += year < 30 ? 2000 : 1900;
    }
    return civilDays(year, Number(parts.m), Number(parts.d));
  }

  /// A typed time as days; an hour past 23 or a sign is a duration, without
  /// `AM`/`PM`.
  function readTime(text) {
    var time = TIME.exec(text);
    if (time === null) {
      return null;
    }
    var hours = Number(time[2]);
    var minutes = Number(time[3]);
    var seconds = Number(time[4] || 0) + Number("0." + (time[5] || "0"));
    if (minutes > 59 || seconds >= 60) {
      return null;
    }
    if (time[6] !== undefined) {
      if (time[1] !== "" || hours < 1 || hours > 12) {
        return null;
      }
      hours = (hours % 12) + (time[6].toLowerCase() === "p" ? 12 : 0);
    }
    var result = (hours * 3600 + minutes * 60 + seconds) / 86400;
    return time[1] !== "" ? -result : result;
  }

  /// A date, a time, or a date and a time, or null.
  function readMoment(text) {
    var date = readDate(text);
    if (date !== null) {
      return { type: "date", number: date };
    }
    var time = readTime(text);
    if (time !== null) {
      return { type: "time", number: time };
    }
    // a date can hold spaces, `2. 1. 2025`, and so can a time, `6:00 PM`
    for (var split = text.search(/\s/); split > 0; ) {
      date = readDate(text.slice(0, split));
      time = readTime(text.slice(split).trim());
      if (date !== null && time !== null && time >= 0 && time < 1) {
        return { type: "date", number: date + time };
      }
      var next = text.slice(split + 1).search(/\s/);
      split = next < 0 ? -1 : split + 1 + next;
    }
    return null;
  }

  /// Parses numbers and, on styled sheets, locale dates/times. Other input
  /// stays text; a leading apostrophe forces text.
  function parse(text) {
    var quoted = text.charAt(0) === "'";
    var content = quoted ? text.slice(1) : text;
    if (content === "") {
      return { type: "empty" };
    }
    var number = Number(content.replace(DECIMAL, "."));
    if (!quoted && NUMBER.test(content) && Number.isFinite(number)) {
      return {
        type: "number",
        number: number,
        text: content,
      };
    }
    var moment = quoted || !styles ? null : readMoment(content);
    if (moment !== null) {
      moment.text = content;
      return moment;
    }
    return { type: "string", text: content };
  }

  function twoDigits(value) {
    return (value < 10 ? "0" : "") + value;
  }

  /// @p seconds as `h:mm`, with seconds where it has them.
  function spellTime(seconds) {
    var magnitude = Math.abs(seconds);
    var result =
      (seconds < 0 ? "-" : "") +
      Math.floor(magnitude / 3600) +
      ":" +
      twoDigits(Math.floor(magnitude / 60) % 60);
    return magnitude % 60 !== 0
      ? result + ":" + twoDigits(magnitude % 60)
      : result;
  }

  /// A date or a time as the locale writes one, which `parse` reads back.
  function spellMoment(value) {
    // rounded as a whole, so 23:59:59.6 is the next day
    var seconds = Math.round(value.number * 86400);
    if (value.type === "time") {
      return spellTime(seconds);
    }
    var days = Math.floor(seconds / 86400);
    var date = DATE_FORMAT.format(new Date((days - 25569) * 86400000));
    var time = seconds - days * 86400;
    return time > 0 ? date + " " + spellTime(time) : date;
  }

  /// What the editor opens on: a formatted number its value, and a string
  /// that would not read back as itself the `'` that keeps it one.
  function spell(value) {
    if (value.type === "empty") {
      return "";
    }
    var text = value.text;
    if (value.type === "number" && !same(parse(text), value)) {
      return String(value.number).replace(".", DECIMAL);
    }
    if (
      (value.type === "date" || value.type === "time") &&
      !same(parse(text), value)
    ) {
      return spellMoment(value);
    }
    if (
      value.type === "string" &&
      (text.charAt(0) === "=" || !same(parse(text), value))
    ) {
      return "'" + text;
    }
    return text;
  }

  /// Replaces the characters @p swaps names outside a quoted string, a quoted
  /// sheet name and an array constant, which keeps the spelling of an `<f>`.
  /// The decimal sign @p decimal is one only ahead of a digit, so `STDEV.S`
  /// keeps its dot.
  function swapped(text, swaps, decimal) {
    var result = "";
    var quote = null;
    var braces = 0;
    for (var i = 0; i < text.length; ++i) {
      var c = text.charAt(i);
      if (quote !== null) {
        quote = c === quote ? null : quote;
      } else if (c === '"' || c === "'") {
        quote = c;
      } else if (c === "{" || c === "}") {
        braces += c === "{" ? 1 : -1;
      } else if (
        braces === 0 &&
        swaps[c] !== undefined &&
        (c !== decimal || /[0-9]/.test(text.charAt(i + 1)))
      ) {
        c = swaps[c];
      }
      result += c;
    }
    return result;
  }

  /// A formula as an `<f>` spells it, the way the locale types it: where the
  /// decimal sign is `,`, a `;` goes between the arguments.
  function localized(input) {
    return DECIMAL === ","
      ? swapped(input, { ",": ";", ".": "," }, ".")
      : input;
  }

  /// What @p typed spells as an `<f>` does. `;` between arguments is taken in
  /// every locale.
  function delocalized(typed) {
    return DECIMAL === ","
      ? swapped(typed, { ";": ",", ",": "." }, ",")
      : swapped(typed, { ";": "," }, null);
  }

  /// Two numbers are the same where their values are, whatever the text, and
  /// two dates or times within half a second, the precision `spell` writes.
  function same(one, other) {
    if (one.type !== other.type) {
      return false;
    }
    switch (one.type) {
      case "empty":
        return true;
      case "number":
        return one.number === other.number;
      case "date":
      case "time":
        return Math.abs(one.number - other.number) < 0.5 / 86400;
      default:
        return one.text === other.text;
    }
  }

  /// The formula a cell states, and what it reads, both null for none.
  function formulaOf(cell) {
    return {
      formula: cell.getAttribute("data-odr-formula"),
      reads: cell.getAttribute("data-odr-reads"),
    };
  }

  function setAttribute(cell, name, value) {
    if (value === null) {
      cell.removeAttribute(name);
    } else {
      cell.setAttribute(name, value);
    }
  }

  /// Writes @p value at a position: the cell shows it, and the op joins the
  /// log beside the value it replaced. A formula value states its expression
  /// in the syntax of the file, and shows nothing until it is computed.
  function write(column, row, value) {
    var cell = odr.sheet.cellAt(column, row);
    var before = odr.sheet.valueAt(column, row);
    if (cell === null || before === null) {
      return false;
    }
    // the log replays from the formulas the file states
    readersOf();
    var formulaBefore = formulaOf(cell);
    var formula =
      value.type === "formula"
        ? {
            formula: value.formula,
            reads: formulas.module.reads(
              value.formula,
              formulas.syntax,
              sheet,
              formulas.name,
              namesAfter(coalesced())
            ),
          }
        : { formula: null, reads: null };
    if (
      formula.formula === formulaBefore.formula &&
      (formula.formula !== null || same(before, value))
    ) {
      return false;
    }
    var shownBefore = shownValue(cell, column, row);
    var shown = value.type === "formula" ? { type: "empty" } : value;
    var computedBefore = computed.get(cell);

    // what a repaint computes for the cell, and what an undo puts back
    function apply(state, show, result) {
      if (!odr.sheet.showValue(column, row, show)) {
        return false;
      }
      setAttribute(cell, "data-odr-formula", state.formula);
      setAttribute(cell, "data-odr-reads", state.reads);
      if (result === undefined) {
        computed.delete(cell);
      } else {
        computed.set(cell, result);
      }
      return true;
    }

    if (!apply(formula, shown, undefined)) {
      return false;
    }
    var op = {
      op: "setCell",
      sheet: sheet,
      column: column,
      row: row,
      value: value,
    };
    if (formula.formula !== null) {
      written.set(op, { cell: cell, reads: formula.reads });
    }
    history.push({
      ops: [op],
      undo: function () {
        apply(formulaBefore, shownBefore, computedBefore);
      },
      redo: function () {
        apply(formula, shown, undefined);
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

  /// The boxes of `data-odr-reads` @p stated that lie in this sheet.
  function boxesOf(stated) {
    var boxes = [];
    var groups = stated === "" ? [] : stated.split(" ");
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
    return boxes;
  }

  /// The columns and rows an array formula fills from @p cell, null for a
  /// cell that states none.
  function arrayOf(cell) {
    var stated = cell.getAttribute("data-odr-array");
    if (stated === null) {
      return null;
    }
    var parts = stated.split(",");
    return { columns: Number(parts[0]), rows: Number(parts[1]) };
  }

  /// The positions @p entry fills: its own, and every other cell of its
  /// array formula.
  function positionsOf(entry) {
    if (entry.array === null) {
      return [entry.at];
    }
    var result = [];
    for (var row = 0; row < entry.array.rows; ++row) {
      for (var column = 0; column < entry.array.columns; ++column) {
        result.push({
          column: entry.at.column + column,
          row: entry.at.row + row,
        });
      }
    }
    return result;
  }

  /// What an array formula's @p result puts at (@p column, @p row) of its
  /// range: a single row or column repeats, and past the result is `#N/A`.
  function spread(result, column, row) {
    var c = result.columns === 1 ? 0 : column;
    var r = result.rows === 1 ? 0 : row;
    if (c >= result.columns || r >= result.rows) {
      return { type: "error", text: "#N/A" };
    }
    return result.cells[r * result.columns + c];
  }

  /// Every formula, including one with no reads: ROW() and COLUMN() still
  /// change when their own cell moves.
  function readerOf(cell) {
    var at = odr.sheet.positionOf(cell);
    var stated = cell.getAttribute("data-odr-reads");
    var array = arrayOf(cell);
    if (at === null || !cell.hasAttribute("data-odr-formula")) {
      return null;
    }
    return {
      cell: cell,
      formula: cell.getAttribute("data-odr-formula"),
      moves: [],
      array: array,
      at: at,
      reads: boxesOf(stated || ""),
    };
  }

  function readersOf() {
    if (readers !== null) {
      return readers;
    }
    readers = [];
    var cells = table.querySelectorAll("td[data-odr-formula]");
    for (var i = 0; i < cells.length; ++i) {
      var entry = readerOf(cells[i]);
      if (entry !== null) {
        readers.push(entry);
      }
    }
    return readers;
  }

  /// The cell a `setCell` op of a formula wrote, and what the formula reads
  /// there, so the log can replay it.
  var written = new WeakMap();

  var movedFormulas = new Map();

  /// @p formula after the row or column op @p op, null where the page cannot
  /// move it. A reference that names no sheet is on @p own, this sheet where
  /// it is left out, and on none where it is null.
  function moved(formula, op, own) {
    if (formula === null || formulas === null || formulas.module === null) {
      return null;
    }
    var on = own === undefined ? formulas.name : own;
    var key = JSON.stringify([formula, on, op]);
    if (!movedFormulas.has(key)) {
      movedFormulas.set(
        key,
        formulas.module.move(formula, formulas.syntax, formulas.name, op, on)
      );
    }
    return movedFormulas.get(key);
  }

  /// The formula of @p entry after its row and column ops, null where the
  /// page cannot move it. The ops wait in the entry until the module loads.
  function movedFormula(entry) {
    return entry.moves.reduce(function (formula, op) {
      return moved(formula, op);
    }, entry.formula);
  }

  /// The names a formula here reads, as the row and column ops of the saves
  /// and of @p ops moved them.
  function namesAfter(ops) {
    var all = formulas.savedMoves.concat(ops);
    return formulas.names.map(function (name) {
      var expression = name.expression;
      all.forEach(function (op) {
        if (STRUCTURE_OPS[op.op] !== undefined && op.sheet === sheet) {
          expression = moved(
            expression,
            op,
            name.sheet === null ? null : undefined
          );
        }
      });
      return { name: name.name, sheet: name.sheet, expression: expression };
    });
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

  /// Where a row or column @p index goes under a row or column op along its
  /// axis, null where a delete takes it. An unbounded one stays unbounded.
  function movedIndex(op, index) {
    var edge = op[STRUCTURE_OPS[op.op]];
    if (index < edge) {
      return index;
    }
    if (op.op.indexOf("insert") === 0) {
      return index + op.count;
    }
    return index < edge + op.count ? null : index - op.count;
  }

  /// What @p first to @p last span after the op, as the library moves a
  /// range: null where a delete takes all of them.
  function movedSpan(op, first, last) {
    var edge = op[STRUCTURE_OPS[op.op]];
    var inserts = op.op.indexOf("insert") === 0;
    if (!inserts && first >= edge && last < edge + op.count) {
      return null;
    }
    var from = movedIndex(op, first);
    var to = movedIndex(op, last);
    return [from === null ? edge : from, to === null ? edge - 1 : to];
  }

  /// Moves formula positions and dependencies through @p ops, marking changed
  /// inputs and omitting deleted cells.
  function walk(ops) {
    var entries = readersOf().map(function (entry) {
      return {
        cell: entry.cell,
        formula: entry.formula,
        moves: entry.moves.slice(),
        array: entry.array,
        at: { column: entry.at.column, row: entry.at.row },
        reads: entry.reads.map(function (box) {
          return box.slice();
        }),
        marked: false,
      };
    });
    ops.forEach(function (op) {
      if (op.op === "setCell") {
        entries = entries.filter(function (entry) {
          entry.marked = entry.marked || readsAny(entry, [op]);
          return entry.at.column !== op.column || entry.at.row !== op.row;
        });
        // a formula written into the cell is new, so it computes too
        var typed = written.get(op);
        if (typed !== undefined) {
          entries.push({
            cell: typed.cell,
            formula: op.value.formula,
            moves: [],
            array: null,
            at: { column: op.column, row: op.row },
            reads: boxesOf(typed.reads || ""),
            marked: true,
          });
        }
        return;
      }
      var axis = STRUCTURE_OPS[op.op];
      if (axis === undefined || op.sheet !== sheet) {
        return;
      }
      // a box is [first column, last column, first row, last row]
      var low = axis === "row" ? 2 : 0;
      entries = entries.filter(function (entry) {
        // As in native recalculation: position-sensitive formulas can change
        // even when the values they read do not.
        entry.marked = true;
        entry.at[axis] = movedIndex(op, entry.at[axis]);
        entry.moves.push(op);
        entry.reads = entry.reads
          .map(function (box) {
            var span = movedSpan(op, box[low], box[low + 1]);
            if (span === null) {
              return null;
            }
            var moved = box.slice();
            moved[low] = span[0];
            moved[low + 1] = span[1];
            return moved;
          })
          .filter(function (box) {
            return box !== null;
          });
        return entry.at[axis] !== null;
      });
    });
    return entries;
  }

  /// What the boolean cells of the page show, true first, empty where none
  /// does: the file spelled them in the language of whoever saved it.
  function shownBooleanWords() {
    var shown = ["", ""];
    table
      .querySelectorAll('td[data-odr-type="boolean"]')
      .forEach(function (cell) {
        var i = cell.getAttribute("data-odr-value") === "1" ? 0 : 1;
        if (shown[i] === "") {
          shown[i] = cell.textContent.trim();
        }
      });
    return shown;
  }

  /// The evaluator of `wasm/formula`, where the page carries one: `<body>`
  /// states the syntax and the settings it computes with. `module` is null
  /// until it has loaded.
  var formulas = (function () {
    var syntax = document.body.getAttribute("data-odr-formula-syntax");
    if (syntax === null || typeof window.createOdrFormula !== "function") {
      return null;
    }
    var settings = {};
    var names = [];
    try {
      settings = JSON.parse(
        document.body.getAttribute("data-odr-formula-settings") || "{}"
      );
      names = JSON.parse(
        document.body.getAttribute("data-odr-formula-names") || "[]"
      );
    } catch (e) {
      return null;
    }
    return {
      syntax: syntax,
      settings: settings,
      // the names of the whole document, and the ones local to this sheet
      names: names
        .filter(function (name) {
          return name.sheet === null || name.sheet === sheet;
        })
        .map(function (name) {
          return {
            name: name.name.toLowerCase(),
            sheet: name.sheet,
            expression: name.expression,
          };
        }),
      // the row and column ops of the saves, which wait for the module
      savedMoves: [],
      locale: document.body.getAttribute("data-odr-locale") || "",
      booleanWords: null,
      name: (table.getAttribute("data-odr-sheet-name") || "").toLowerCase(),
      module: null,
      loading: null,
    };
  })();

  /// Loads the evaluator once. Resolves to whether the page computes.
  function loadFormulas() {
    if (formulas === null) {
      return Promise.resolve(false);
    }
    if (formulas.loading === null) {
      formulas.loading = window.createOdrFormula().then(
        function (module) {
          formulas.module = module;
          // nothing is computed yet, so the cells show what the file wrote
          var shown = shownBooleanWords();
          formulas.booleanWords = module.booleanWords(
            formulas.locale,
            shown[0],
            shown[1]
          );
          repaintStale();
          return true;
        },
        function () {
          formulas = null;
          return false;
        }
      );
    }
    return formulas.loading;
  }

  /// What a cell holds as the evaluator reads it, from what the page shows.
  function shownValue(cell, column, row) {
    var type = cell.getAttribute("data-odr-type");
    var value = odr.sheet.valueAt(column, row);
    if (type === "boolean") {
      return {
        type: "boolean",
        number: cell.getAttribute("data-odr-value") === "1" ? 1 : 0,
        text: value.text,
      };
    }
    if (type === "error") {
      return { type: "error", text: value.text };
    }
    return value;
  }

  function toFormula(value) {
    switch (value.type) {
      case "number":
      case "time":
        return { type: "number", number: value.number };
      case "date":
        return {
          type: "number",
          number: formulas.module.serial(value.number, formulas.settings),
        };
      case "boolean":
        return { type: "boolean", boolean: value.number !== 0 };
      case "error":
      case "string":
        return { type: value.type, text: value.text };
      default:
        return { type: "empty" };
    }
  }

  /// A result as `showValue` takes it, in the format the cell states.
  function toShown(cell, result) {
    if (result.type === "number") {
      var code = cell.getAttribute("data-odr-format") || "";
      var category = formulas.module.category(code);
      if (category === null) {
        return null;
      }
      var days = formulas.module.days(result.number, formulas.settings);
      var text = formulas.module.format(
        category === "date" ? formulas.module.serial(days, {}) : result.number,
        code,
        formulas.locale,
        false
      );
      if (text === null) {
        return null;
      }
      return {
        type: category,
        number: category === "date" ? days : result.number,
        text: text,
      };
    }
    if (result.type === "boolean") {
      return {
        type: "boolean",
        number: result.boolean ? 1 : 0,
        text: formulas.booleanWords[result.boolean ? 0 : 1],
      };
    }
    if (result.type === "empty") {
      return { type: "empty" };
    }
    return { type: result.type, text: result.text };
  }

  /// The deepest a formula cell the page has no result for computes inside
  /// another. Each level nests an evaluation in the wasm stack.
  var NESTING = 16;

  /// For each of the @p marked entries, the ones it reads, itself too.
  function readsOf(marked) {
    var byPosition = new Map();
    marked.forEach(function (entry, i) {
      positionsOf(entry).forEach(function (at) {
        byPosition.set(at.column + "," + at.row, i);
      });
    });
    return marked.map(function (entry) {
      var seen = new Set();
      entry.reads.forEach(function (box) {
        var area = (box[1] - box[0] + 1) * (box[3] - box[2] + 1);
        if (area <= marked.length) {
          for (var row = box[2]; row <= box[3]; ++row) {
            for (var column = box[0]; column <= box[1]; ++column) {
              var j = byPosition.get(column + "," + row);
              if (j !== undefined) {
                seen.add(j);
              }
            }
          }
          return;
        }
        marked.forEach(function (other, j) {
          if (readsAny({ reads: [box] }, positionsOf(other))) {
            seen.add(j);
          }
        });
      });
      return seen;
    });
  }

  /// The @p marked entries, each after the marked ones it reads, so no
  /// evaluation nests in another. The ones this cannot order come last: a
  /// cycle, and a read of all of the sheet that looks like one.
  function inOrder(marked) {
    var dependents = marked.map(function () {
      return [];
    });
    var waits = marked.map(function () {
      return 0;
    });
    readsOf(marked).forEach(function (seen, i) {
      // a cell has no result while it computes, so reading itself gives
      // no answer, and a read of all its sheet still computes
      seen.forEach(function (j) {
        if (j !== i) {
          dependents[j].push(i);
          ++waits[i];
        }
      });
    });
    var result = [];
    var ready = [];
    waits.forEach(function (count, i) {
      if (count === 0) {
        ready.push(i);
      }
    });
    while (ready.length > 0) {
      var next = ready.pop();
      result.push(marked[next]);
      dependents[next].forEach(function (i) {
        if (--waits[i] === 0) {
          ready.push(i);
        }
      });
    }
    marked.forEach(function (entry, i) {
      if (waits[i] > 0) {
        result.push(entry);
      }
    });
    return result;
  }

  /// The groups of the @p marked entries that read each other, each after
  /// the groups it reads (Kosaraju), as a recalculation iterates them.
  function cyclesOf(marked) {
    var reads = readsOf(marked);
    var readers = marked.map(function () {
      return [];
    });
    reads.forEach(function (seen, i) {
      seen.forEach(function (j) {
        readers[j].push(i);
      });
    });
    var finished = [];
    var seen = marked.map(function () {
      return false;
    });
    marked.forEach(function (entry, start) {
      if (seen[start]) {
        return;
      }
      seen[start] = true;
      var stack = [[start, Array.from(reads[start])]];
      while (stack.length > 0) {
        var top = stack[stack.length - 1];
        if (top[1].length > 0) {
          var j = top[1].pop();
          if (!seen[j]) {
            seen[j] = true;
            stack.push([j, Array.from(reads[j])]);
          }
        } else {
          finished.push(top[0]);
          stack.pop();
        }
      }
    });
    // over who reads each, latest finished first, gives the groups that
    // nothing reads first, so the list turns over at the end
    var placed = marked.map(function () {
      return false;
    });
    var result = [];
    finished.reverse().forEach(function (start) {
      if (placed[start]) {
        return;
      }
      placed[start] = true;
      var group = [];
      var pending = [start];
      while (pending.length > 0) {
        var i = pending.pop();
        group.push(i);
        readers[i].forEach(function (j) {
          if (!placed[j]) {
            placed[j] = true;
            pending.push(j);
          }
        });
      }
      if (group.length > 1 || reads[start].has(start)) {
        group.sort(function (a, b) {
          var one = marked[a].at;
          var other = marked[b].at;
          return one.row - other.row || one.column - other.column;
        });
        result.push(
          group.map(function (i) {
            return marked[i];
          })
        );
      }
    });
    return result.reverse();
  }

  /// How far apart two steps of a cycle leave a cell: infinite where they
  /// change what it is.
  function difference(before, after) {
    if (before.type === "number" && after.type === "number") {
      return Math.abs(after.number - before.number);
    }
    return JSON.stringify(before) === JSON.stringify(after) ? 0 : Infinity;
  }

  /// Computes the marked formula cells of @p entries. Returns each marked
  /// cell's result, null where the evaluator has none: also in a cycle.
  function compute(entries, names) {
    var byCell = new Map();
    // each other cell of an array formula's range, with its place in it
    var owners = new Map();
    entries.forEach(function (entry) {
      byCell.set(entry.cell, entry);
      positionsOf(entry)
        .slice(1)
        .forEach(function (at) {
          var cell = odr.sheet.cellAt(at.column, at.row);
          if (cell !== null) {
            owners.set(cell, {
              entry: entry,
              column: at.column - entry.at.column,
              row: at.row - entry.at.row,
            });
          }
        });
    });
    var results = new Map();
    var computing = new Set();
    // the cells a first pass met while they computed
    var circular = new Set();
    var depth = 0;
    var extent = null;

    // `NOW` reads the moment of the edit, in days since 1899-12-30 local time
    var moment = new Date();
    var settings = Object.assign({}, formulas.settings, {
      now:
        (moment.getTime() - moment.getTimezoneOffset() * 60000) / 86400000 +
        25569,
    });

    function evaluate(formula, column, row) {
      if (formula === null) {
        return null;
      }
      return formulas.module.evaluate(
        formula,
        formulas.syntax,
        sheet,
        column,
        row,
        settings,
        source
      );
    }

    // `{columns, rows, cells}` for the range an array formula fills
    function evaluateArray(formula, column, row) {
      if (formula === null) {
        return null;
      }
      return formulas.module.evaluateArray(
        formula,
        formulas.syntax,
        sheet,
        column,
        row,
        settings,
        source
      );
    }

    var source = {
      // a name local to the sheet before one of the whole document
      name: function (name, at) {
        var key = name.toLowerCase();
        var global = null;
        for (var i = 0; i < names.length; ++i) {
          if (names[i].name !== key) {
            continue;
          }
          if (names[i].sheet === at) {
            return names[i].expression;
          }
          if (names[i].sheet === null && global === null) {
            global = names[i].expression;
          }
        }
        return global;
      },
      sheet: function (name) {
        return name.toLowerCase() === formulas.name ? sheet : null;
      },
      cell: function (at, column, row) {
        if (at !== sheet) {
          return null;
        }
        var cell = odr.sheet.cellAt(column, row);
        if (cell === null) {
          // the page shows all of the sheet, or not what lies past the cut
          return cut === null ? { type: "empty" } : null;
        }
        var position = odr.sheet.positionOf(cell);
        if (position.column !== column || position.row !== row) {
          return { type: "empty" };
        }
        if (iterated.has(cell)) {
          return iterated.get(cell);
        }
        var entry = byCell.get(cell);
        var owner = owners.get(cell);
        if (
          owner === undefined &&
          entry !== undefined &&
          entry.array !== null
        ) {
          owner = { entry: entry, column: 0, row: 0 };
        }
        if (owner !== undefined && owner.entry.marked) {
          var array = results.has(owner.entry.cell)
            ? results.get(owner.entry.cell)
            : computeEntry(owner.entry);
          return array === null
            ? null
            : spread(array, owner.column, owner.row);
        }
        if (entry !== undefined && entry.marked) {
          return results.has(cell) ? results.get(cell) : computeEntry(entry);
        }
        var shown = shownValue(cell, column, row);
        if (cell.hasAttribute("data-odr-formula") && shown.type === "empty") {
          // a formula without a result, which the file left to a reader
          if (depth >= NESTING) {
            return null;
          }
          ++depth;
          var computed = evaluate(
            entry !== undefined
              ? entry.formula
              : cell.getAttribute("data-odr-formula"),
            column,
            row
          );
          --depth;
          return computed;
        }
        return toFormula(shown);
      },
      extent: function () {
        if (cut !== null) {
          return { columns: cut.columns, rows: cut.rows };
        }
        if (extent === null) {
          extent = { columns: 0, rows: 0 };
          table.querySelectorAll("td").forEach(function (cell) {
            var at = odr.sheet.positionOf(cell);
            if (at !== null) {
              extent.rows = Math.max(extent.rows, at.row + 1);
              extent.columns = Math.max(extent.columns, at.column + 1);
            }
          });
        }
        return extent;
      },
    };

    // a cell has no result while it computes, so a cycle has no answer
    function computeEntry(entry) {
      if (computing.has(entry.cell)) {
        circular.add(entry.cell);
        return null;
      }
      if (depth >= NESTING) {
        return null;
      }
      computing.add(entry.cell);
      ++depth;
      var result =
        entry.array !== null
          ? evaluateArray(entry.formula, entry.at.column, entry.at.row)
          : evaluate(entry.formula, entry.at.column, entry.at.row);
      --depth;
      computing.delete(entry.cell);
      results.set(entry.cell, result);
      return result;
    }

    // the cells of the cycle a step computes, and the value each holds now
    var iterated = new Map();

    // a cycle the document iterates, as a save does: in reading order, from
    // what the file holds, until no cell changes by the difference. An ods
    // write drops the result of what an edit reaches, so a save starts from 0
    function iterate(group) {
      var iteration = formulas.settings.iteration;
      group.forEach(function (entry) {
        var held =
          formulas.syntax === "opendocument"
            ? { type: "empty" }
            : toFormula(
                computed.has(entry.cell)
                  ? computed.get(entry.cell)
                  : shownValue(entry.cell, entry.at.column, entry.at.row)
              );
        iterated.set(
          entry.cell,
          held.type === "empty" ? { type: "number", number: 0 } : held
        );
      });
      var converged = false;
      var answered = true;
      for (
        var step = 0;
        step < iteration.steps && !converged && answered;
        ++step
      ) {
        var change = 0;
        for (var i = 0; i < group.length && answered; ++i) {
          var entry = group[i];
          var value =
            entry.array === null
              ? evaluate(entry.formula, entry.at.column, entry.at.row)
              : null;
          if (value === null) {
            answered = false;
          } else {
            change = Math.max(
              change,
              difference(iterated.get(entry.cell), value)
            );
            iterated.set(entry.cell, value);
          }
        }
        converged = answered && change < iteration.difference;
      }
      // LibreOffice states `Err:523` past the steps, and Excel keeps the last
      var kept =
        converged || (answered && formulas.settings.dialect !== "libreoffice");
      group.forEach(function (entry) {
        results.set(entry.cell, kept ? iterated.get(entry.cell) : null);
        iterated.delete(entry.cell);
      });
    }

    var marked = entries.filter(function (entry) {
      return entry.marked;
    });
    function computeAll() {
      inOrder(marked).forEach(function (entry) {
        if (!results.has(entry.cell)) {
          computeEntry(entry);
        }
      });
    }
    computeAll();
    // as a save does: only a group the first pass met a cycle in iterates,
    // since a cell that reads all of its sheet reads itself only as stated
    if (formulas.settings.iteration && circular.size > 0) {
      var cycles = cyclesOf(marked).filter(function (group) {
        return group.some(function (entry) {
          return circular.has(entry.cell);
        });
      });
      results.clear();
      cycles.forEach(iterate);
      computeAll();
    }
    return results;
  }

  /// The formula cells a result went into, with what they showed before.
  var computed = new Map();

  function showResult(entry, value) {
    if (!computed.has(entry.cell)) {
      computed.set(
        entry.cell,
        shownValue(entry.cell, entry.at.column, entry.at.row)
      );
    }
    odr.sheet.showValue(entry.at.column, entry.at.row, value);
  }

  /// Puts back what the cells showed, where no result stands in for it now.
  function restoreComputed(kept) {
    computed.forEach(function (before, cell) {
      if (kept.has(cell)) {
        return;
      }
      var at = odr.sheet.positionOf(cell);
      if (at !== null) {
        odr.sheet.showValue(at.column, at.row, before);
      }
      computed.delete(cell);
    });
  }

  var stale = [];
  var staleKey = "";

  /// The marks follow the log, not the last write, so an undo takes back what
  /// it made stale and a save clears them with the log. A formula cell
  /// reading a marked one is marked too. Where the page computes, a marked
  /// cell shows its result instead, and only one without an answer is stale.
  function repaintStale() {
    var ops = coalesced();
    var entries = walk(ops);
    var frontier = entries.filter(function (entry) {
      return entry.marked;
    });
    while (frontier.length > 0) {
      var positions = [];
      frontier.forEach(function (entry) {
        positions.push.apply(positions, positionsOf(entry));
      });
      frontier = entries.filter(function (entry) {
        return !entry.marked && readsAny(entry, positions);
      });
      frontier.forEach(function (entry) {
        entry.marked = true;
      });
    }

    for (var j = 0; j < stale.length; ++j) {
      stale[j].classList.remove("odr-sheet-stale");
    }
    stale = [];

    var marked = entries.filter(function (entry) {
      return entry.marked;
    });
    if (marked.length > 0) {
      loadFormulas();
    }
    // the page opens and computes a formula as the edits moved it
    entries.forEach(function (entry) {
      entry.formula = movedFormula(entry);
      if (
        entry.formula !== null &&
        entry.formula !== entry.cell.getAttribute("data-odr-formula")
      ) {
        entry.cell.setAttribute("data-odr-formula", entry.formula);
      }
    });
    var computes = formulas !== null && formulas.module !== null;
    // the results go in after every cell is computed, so none reads another
    // one's new text
    var answers = new Map();
    if (computes) {
      try {
        answers = compute(entries, namesAfter(ops));
      } catch (e) {
        // a trap can leave the module's memory broken, so it computes no more
        formulas = null;
      }
    }
    var shown = new Set();
    var cells = [];
    marked.forEach(function (entry) {
      var result = answers.get(entry.cell);
      positionsOf(entry).forEach(function (at) {
        var cell = odr.sheet.cellAt(at.column, at.row);
        if (cell === null) {
          return;
        }
        var element =
          result === undefined || result === null
            ? null
            : entry.array === null
              ? result
              : spread(
                  result,
                  at.column - entry.at.column,
                  at.row - entry.at.row
                );
        var value = element === null ? null : toShown(cell, element);
        if (value !== null) {
          showResult({ cell: cell, at: at }, value);
          shown.add(cell);
          return;
        }
        cell.classList.add("odr-sheet-stale");
        stale.push(cell);
        cells.push({ column: at.column, row: at.row });
      });
    });
    restoreComputed(shown);

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
    // a cell in a far chunk has no geometry until the chunk is laid out
    if (odr.chunks !== undefined) {
      odr.chunks.show(cell);
    }
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
    grow();
  }

  /// A line typed past the cell's height makes the overlay taller.
  function grow() {
    var cell = odr.sheet.cellAt(editingAt.column, editingAt.row);
    overlay.style.height = cell.offsetHeight + "px";
    overlay.style.height =
      Math.max(cell.offsetHeight, overlay.scrollHeight) + "px";
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

    // typed input replaces the formula, so only an opened one needs the module
    var stated = typed === null ? cell.getAttribute("data-odr-formula") : null;
    var input =
      stated !== null && formulas !== null && formulas.module !== null
        ? formulas.module.toInput(
            stated,
            formulas.syntax,
            formulas.locale
          )
        : null;
    if (stated !== null && input === null) {
      refuse("formulaInput", column, row);
      return false;
    }
    var value = odr.sheet.valueAt(column, row);
    editingAt = { column: column, row: row };
    overlay = document.createElement("textarea");
    overlay.className = "odr-sheet-editor";
    overlay.value =
      typed !== null
        ? typed
        : stated !== null
          ? "=" + localized(input)
          : spell(value);
    // in the page first, so `place` can measure the lines it holds
    document.body.appendChild(overlay);
    place(cell);
    overlay.addEventListener("keydown", overlayKey);
    overlay.addEventListener("input", grow);
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
  /// formula the page cannot take is refused rather than written, and leaves
  /// the editor open: without the evaluator, and where it does not parse.
  function commit(columns, rows) {
    if (overlay === null) {
      return false;
    }
    var text = overlay.value;
    var at = editingAt;
    var value = null;
    if (text.charAt(0) === "=") {
      if (formulas === null || formulas.module === null) {
        refuse("formulaInput", at.column, at.row);
        return false;
      }
      var expression = formulas.module.toFile(
        delocalized(text.slice(1)),
        formulas.syntax,
        formulas.locale
      );
      if (expression === null) {
        refuse("formulaInvalid", at.column, at.row);
        return false;
      }
      value = { type: "formula", formula: expression };
    }
    close();
    write(at.column, at.row, value !== null ? value : parse(text));
    if (!odr.sheet.pin({ column: at.column + columns, row: at.row + rows })) {
      odr.sheet.pin({ column: at.column, row: at.row });
      refuseCut(at.column + columns, at.row + rows);
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
    } else if (
      event.key === "Enter" &&
      (event.altKey || event.ctrlKey || event.metaKey)
    ) {
      // a line break, as Alt+Enter in Excel and Ctrl+Enter in Calc
      overlay.setRangeText(
        "\n",
        overlay.selectionStart,
        overlay.selectionEnd,
        "end"
      );
      grow();
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

  /// Whether a key at @p target types into a field the page or the host owns.
  function typingIn(target) {
    return (
      target !== null &&
      (target.isContentEditable ||
        /^(INPUT|TEXTAREA|SELECT)$/.test(target.tagName))
    );
  }

  /// What a pinned cell does with a key when no editor is open. Captured, so
  /// these never reach the pin and the sort beneath. The chord is `editing.js`'s.
  function pinnedKey(event) {
    if (
      !odr.editing.isEnabled() ||
      overlay !== null ||
      event.ctrlKey ||
      event.metaKey ||
      event.altKey ||
      typingIn(event.target)
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
      // shift with an arrow moves the focus, anything else the pin
      var widens = event.shiftKey && arrows[event.key] !== undefined;
      var from = widens ? odr.sheet.selection().focus : at;
      var to = { column: from.column + step[0], row: from.row + step[1] };
      if (!(widens ? odr.sheet.select(to) : odr.sheet.pin(to))) {
        refuseCut(to.column, to.row);
      }
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
  /// pointer. `HtmlConfig::sheet_edit_on_click` states the start value.
  var onClick = document.body.getAttribute("data-odr-sheet-edit-on-click");
  var options = { editOnClick: onClick === null ? null : onClick === "true" };

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

  /// Loads the evaluator the page carries, which an edit or `enable` does
  /// too. Resolves to whether the page computes its formula cells.
  odr.editing.loadFormulas = loadFormulas;

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

  // A csv states no style.
  var styles = document.body.getAttribute("data-odr-sheet-styles") !== "false";
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
      // null follows the value type, also after a later write changes it
      holders.concat(holders[0] === cell ? [] : [cell]).forEach(function (node) {
        node.style.textAlign =
          style.align !== null
            ? style.align
            : node === cell
              ? "var(--odr-align-by-type)"
              : "inherit";
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
        return (
          value === null ||
          value === "left" ||
          value === "center" ||
          value === "right"
        );
      }
      return false;
    });
  }

  /// A style op on a cell, or on a row or a column where @p column or @p row
  /// is null.
  function styleOp(kind, column, row, style) {
    var op = { op: kind, sheet: sheet };
    if (column !== null) {
      op.column = column;
    }
    if (row !== null) {
      op.row = row;
    }
    op.style = Object.assign({}, style);
    return op;
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
    if (!styles) {
      odr.editing.refuse("unsupportedEdit", { sheet: sheet });
      return false;
    }
    var cells = odr.sheet.selectedCells();
    if (
      cells.length === 0 ||
      Object.keys(style).length === 0 ||
      !valid(style)
    ) {
      odr.editing.refuse("unsupportedEdit", { sheet: sheet });
      return false;
    }
    var befores = [];
    var afters = [];
    var ops = [];
    var rows = new Set();
    // a header stands for its whole row or column, past the rendered extent
    var pin = odr.sheet.pinned();
    var header = pin.row === null || pin.column === null;
    if (header) {
      ops.push(
        styleOp(
          pin.row === null ? "setColumnStyle" : "setRowStyle",
          pin.column,
          pin.row,
          style
        )
      );
    }
    cells.forEach(function (at) {
      befores.push(snapshot(at.cell));
      paintCell(at.cell, style);
      afters.push(snapshot(at.cell));
      if (!header) {
        ops.push(styleOp("setCellStyle", at.column, at.row, style));
      }
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

  /// Whether @p count rows or columns at @p index cut through the range of an
  /// array formula, which the library refuses as it refuses a cut merge.
  function cutsArray(axis, index, count, insert) {
    return walk(coalesced()).some(function (entry) {
      if (entry.array === null) {
        return false;
      }
      var first = entry.at[axis];
      var last =
        first + (axis === "row" ? entry.array.rows : entry.array.columns) - 1;
      if (insert) {
        return first < index && index <= last;
      }
      var end = index + count - 1;
      return !(end < first || index > last || (index <= first && end >= last));
    });
  }

  /// Edits the selected rows or columns as one undo step; @p after inserts
  /// after the selection. Captures formulas before the first structural edit.
  function editStructure(axis, insert, after) {
    finish();
    if (!odr.editing.isEnabled()) {
      return false;
    }
    if (!odr.editing.isEditable()) {
      odr.editing.refuse("readOnly", { sheet: sheet });
      return false;
    }
    readersOf();
    var range = odr.sheet.selection();
    var span = range === null ? null : axis === "row" ? range.rows : range.columns;
    var index = span === null ? null : after ? span[1] + 1 : span[0];
    var count = span === null ? 0 : span[1] - span[0] + 1;
    var plural = axis === "row" ? "Rows" : "Columns";
    if (index !== null && cutsArray(axis, index, count, insert)) {
      var refused = { sheet: sheet };
      refused[axis] = index;
      odr.editing.refuse("unsupportedEdit", refused);
      return false;
    }
    var taken =
      index === null ? null : odr.sheet[(insert ? "insert" : "delete") + plural](index, count);
    var detail = { sheet: sheet };
    detail[axis] = index;
    if (taken === null) {
      odr.editing.refuse("unsupportedEdit", detail);
      return false;
    }
    var op = { op: (insert ? "insert" : "delete") + plural, sheet: sheet };
    op[axis] = index;
    op.count = count;
    var takeOut = function () {
      odr.sheet["delete" + plural](index, count);
    };
    var putBack = function () {
      odr.sheet["restore" + plural](index, taken);
    };
    history.push({
      ops: [op],
      undo: insert ? takeOut : putBack,
      redo: insert ? putBack : takeOut,
    });
    undone = [];
    repaintStale();
    odr.editing.changed();
    reportSelection(true);
    return true;
  }

  /// Inserts as many rows as the selection spans, `above` it or `below` it.
  function insertRows(where) {
    if (where !== undefined && where !== "above" && where !== "below") {
      odr.editing.refuse("unsupportedEdit", { sheet: sheet });
      return false;
    }
    return editStructure("row", true, where === "below");
  }

  /// Inserts as many columns as the selection spans, `left` of it or `right`
  /// of it.
  function insertColumns(where) {
    if (where !== undefined && where !== "left" && where !== "right") {
      odr.editing.refuse("unsupportedEdit", { sheet: sheet });
      return false;
    }
    return editStructure("column", true, where === "right");
  }

  var chords = { b: "bold", i: "italic", u: "underline" };

  /// The formatting chords, where the config gives the scripts the shortcuts.
  function chordKey(event) {
    if (
      !odr.editing.isEnabled() ||
      overlay !== null ||
      event.altKey ||
      !(event.ctrlKey || event.metaKey) ||
      typingIn(event.target)
    ) {
      return;
    }
    // a header selected: Shift with `+` inserts before it, `-` deletes, as in
    // Excel
    var pin = odr.sheet.pinned();
    var axis =
      pin === null || (pin.column === null) === (pin.row === null)
        ? null
        : pin.column === null
          ? "row"
          : "column";
    if (axis !== null && (event.key === "+" || (event.shiftKey && event.key === "="))) {
      editStructure(axis, true, false);
    } else if (axis !== null && event.key === "-" && !event.shiftKey) {
      editStructure(axis, false, false);
    } else {
      var property = chords[event.key.toLowerCase()];
      if (property === undefined || odr.sheet.selectedCells().length === 0) {
        return;
      }
      toggle(property);
    }
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
    insertRows: insertRows,
    deleteRows: function () {
      return editStructure("row", false, false);
    },
    insertColumns: insertColumns,
    deleteColumns: function () {
      return editStructure("column", false, false);
    },
    enable: function () {
      loadFormulas();
      reportSelection(true);
      if (cut !== null) {
        odr.editing.refuse("sheetCut", {
          sheet: sheet,
          columns: cut.columns,
          rows: cut.rows,
        });
      }
    },
    committed: function () {
      // what the formula cells and the names read, where the saved rows put it
      readers = walk(coalesced());
      if (formulas !== null) {
        formulas.savedMoves = formulas.savedMoves.concat(
          coalesced().filter(function (op) {
            return STRUCTURE_OPS[op.op] !== undefined;
          })
        );
      }
      // the results shown are the ones the save wrote
      computed.clear();
      history = [];
      undone = [];
      repaintStale();
    },
  });
})();
