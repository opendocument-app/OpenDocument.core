(function () {
  "use strict";

  var table = document.querySelector(".odr-sheet");
  if (table === null) {
    return;
  }

  var merged = table.querySelector("td[colspan],td[rowspan]") !== null;

  var odr = (window.odr = window.odr || {});

  var style = document.createElement("style");
  document.head.appendChild(style);

  var hovered = -1;
  var pinnedColumn = -1;
  var pinnedRow = null;
  var pinnedCell = null;

  // Column 0 is the gutter, which labels no column.
  function columnRule(index, wash, scope) {
    if (index < 1) {
      return "";
    }
    return (
      ".odr-sheet " +
      scope +
      "tr>:nth-child(" +
      (index + 1) +
      "){background-image:linear-gradient(" +
      wash +
      "," +
      wash +
      ")}"
    );
  }

  // The ruler reacts harder than the cells: it is the label being followed.
  function paint() {
    style.textContent =
      columnRule(hovered, "var(--odr-sheet-wash)", "") +
      columnRule(pinnedColumn, "var(--odr-sheet-wash-pinned)", "") +
      columnRule(hovered, "var(--odr-sheet-wash-ruler)", "thead ") +
      columnRule(pinnedColumn, "var(--odr-sheet-wash-ruler)", "thead ");
  }

  // What the wash paints: the cell's place among the ones written beside it,
  // gutter included, which is what `nth-child` counts. Not a position - a
  // merge writes nothing for a covered one, and gets no wash either.
  function rulerColumn(cell) {
    return cell !== null && !merged ? cell.cellIndex : -1;
  }

  // The gutter's label, which names the row wherever a sort has put it.
  function rowOf(tr) {
    return Number(tr.cells[0].textContent) - 1;
  }

  var index = null;

  // Whether a cell above still reaches into @p row.
  function holds(above, row) {
    return above !== undefined && above.last >= row;
  }

  // A row by its label and, where the sheet merges, its cells by position.
  // Walked once: a merged sheet is offered no sort control, so the rows are
  // still in the file's order here and one pass can carry the rowspans down.
  function build() {
    var index = { rows: new Map(), positions: new Map() };
    var covered = [];
    var body = table.tBodies[0];
    for (var i = 0; i < body.rows.length; ++i) {
      var tr = body.rows[i];
      var row = rowOf(tr);
      var line = [];
      index.rows.set(row, { tr: tr, cells: line });
      if (!merged) {
        continue;
      }
      var column = 0;
      for (var j = 1; j < tr.cells.length; ++j) {
        var td = tr.cells[j];
        while (holds(covered[column], row)) {
          line[column] = covered[column].cell;
          ++column;
        }
        var columns = Number(td.getAttribute("colspan") || 1);
        var last = row + Number(td.getAttribute("rowspan") || 1) - 1;
        for (var k = 0; k < columns; ++k) {
          line[column + k] = td;
          covered[column + k] = { last: last, cell: td };
        }
        index.positions.set(td, { column: column, row: row });
        column += columns;
      }
      // A rowspan reaching past the row's last cell covers the rest of it.
      for (; column < covered.length; ++column) {
        if (holds(covered[column], row)) {
          line[column] = covered[column].cell;
        }
      }
    }
    return index;
  }

  function indexed() {
    if (index === null) {
      index = build();
    }
    return index;
  }

  // The `td` at a position, or null past the sheet's extent. A position a
  // merge covers answers with the cell covering it, which is the one the file
  // states and an op names.
  function cellAt(column, row) {
    var entry = indexed().rows.get(row);
    if (entry === undefined || column < 0) {
      return null;
    }
    var cell = merged ? entry.cells[column] : entry.tr.cells[column + 1];
    return cell === undefined ? null : cell;
  }

  // Where a `td` sits, the way an op names it. Null for anything else - a
  // header, a cell of another table.
  function positionOf(cell) {
    if (cell === null || cell.tagName !== "TD") {
      return null;
    }
    if (merged) {
      var position = indexed().positions.get(cell);
      return position === undefined
        ? null
        : { column: position.column, row: position.row };
    }
    return { column: cell.cellIndex - 1, row: rowOf(cell.parentElement) };
  }

  var raisedCell = null;
  var raisedWrapper = null;
  var raisedContent = null;

  // The block the cell writes, or the cell where it writes none. `null` for
  // anything else — a shape, several blocks — which is not raised.
  function boxOf(cell) {
    if (cell.childElementCount === 0) {
      return cell;
    }
    var only = cell.firstElementChild;
    return cell.childElementCount === 1 && only.tagName === "X-P" ? only : null;
  }

  // Past the cell's edge by the spill `translate_sheet` measured, at the edge
  // where it clips, unbounded where it does neither.
  function visibleRight(cell) {
    var style = getComputedStyle(cell);
    var right = cell.getBoundingClientRect().right;
    var inset = /inset\(([^)]*)\)/.exec(style.clipPath || "");
    if (inset !== null) {
      var sides = inset[1].trim().split(/\s+/);
      return sides.length > 1 ? right - parseFloat(sides[1]) : right;
    }
    return style.overflow === "visible" ? Infinity : right;
  }

  // On the text, not the box: what is cut off is the string running past where
  // the cell still paints.
  function cutOff(cell, box) {
    var range = document.createRange();
    range.selectNodeContents(box);
    var ink = range.getBoundingClientRect();
    var rect = cell.getBoundingClientRect();
    return (
      ink.width > 0 &&
      (ink.right > visibleRight(cell) + 1 ||
        (getComputedStyle(cell).overflow !== "visible" &&
          ink.bottom > rect.bottom + 1))
    );
  }

  function lower() {
    if (raisedCell === null) {
      return;
    }
    raisedCell.classList.remove("odr-sheet-raised");
    if (raisedWrapper !== null) {
      while (raisedWrapper.firstChild) {
        raisedCell.insertBefore(raisedWrapper.firstChild, raisedWrapper);
      }
      raisedWrapper.remove();
      raisedWrapper = null;
    }
    raisedContent = null;
    raisedCell = null;
  }

  // Over its neighbours rather than pushing them aside.
  function raise(cell) {
    lower();
    if (cell === null || cell.tagName !== "TD") {
      return;
    }
    var box = boxOf(cell);
    if (box === null || !cutOff(cell, box)) {
      return;
    }
    if (box === cell) {
      raisedWrapper = document.createElement("span");
      raisedWrapper.className = "odr-sheet-raised-box";
      while (cell.firstChild) {
        raisedWrapper.appendChild(cell.firstChild);
      }
      cell.appendChild(raisedWrapper);
      box = raisedWrapper;
    }
    cell.classList.add("odr-sheet-raised");
    raisedCell = cell;
    raisedContent = box;
  }

  function pin(column, row, cell) {
    lower();
    focus = null;
    if (pinnedRow !== null) {
      pinnedRow.classList.remove("odr-sheet-pinned");
    }
    if (pinnedCell !== null) {
      pinnedCell.classList.remove("odr-sheet-pinned-cell");
    }

    pinnedColumn = column;
    pinnedRow = row;
    pinnedCell = cell;

    if (pinnedRow !== null) {
      pinnedRow.classList.add("odr-sheet-pinned");
    }
    if (pinnedCell !== null) {
      pinnedCell.classList.add("odr-sheet-pinned-cell");
      raise(pinnedCell);
    }
    paint();
    paintSelection();
  }

  // The corner a gesture moved, the pin being the other; null while the
  // selection is the pin alone.
  var focus = null;
  var selected = [];

  function lastRow() {
    var last = -1;
    indexed().rows.forEach(function (entry, row) {
      last = Math.max(last, row);
    });
    return last;
  }

  function lastColumn() {
    return table.tHead.rows[0].children.length - 2;
  }

  // The rectangle the pin and the focus span, a header's row or column across
  // the rendered extent, null where nothing is pinned.
  function selectionOf() {
    var at = pinnedPosition();
    if (at === null) {
      return null;
    }
    if (at.column === null && at.row === null) {
      return null;
    }
    if (at.row === null) {
      return {
        columns: [at.column, at.column],
        rows: [0, lastRow()],
        focus: null,
      };
    }
    if (at.column === null) {
      return { columns: [0, lastColumn()], rows: [at.row, at.row], focus: null };
    }
    var to = focus === null ? at : focus;
    return {
      columns: [Math.min(at.column, to.column), Math.max(at.column, to.column)],
      rows: [Math.min(at.row, to.row), Math.max(at.row, to.row)],
      focus: { column: to.column, row: to.row },
    };
  }

  // Every cell of the selection once, at the position an op names it by: a
  // merge answers with its anchor.
  function selectedCells() {
    var range = selectionOf();
    var result = [];
    if (range === null) {
      return result;
    }
    var seen = new Set();
    for (var row = range.rows[0]; row <= range.rows[1]; ++row) {
      for (var column = range.columns[0]; column <= range.columns[1]; ++column) {
        var cell = cellAt(column, row);
        if (cell === null || seen.has(cell)) {
          continue;
        }
        seen.add(cell);
        var position = positionOf(cell);
        result.push({ cell: cell, column: position.column, row: position.row });
      }
    }
    return result;
  }

  // The pin's outline marks a single cell, so only a wider selection is washed.
  function paintSelection() {
    for (var i = 0; i < selected.length; ++i) {
      selected[i].classList.remove("odr-sheet-selected");
    }
    selected = [];
    var cells = selectedCells();
    if (cells.length > 1) {
      for (var j = 0; j < cells.length; ++j) {
        cells[j].cell.classList.add("odr-sheet-selected");
        selected.push(cells[j].cell);
      }
    }
    table.dispatchEvent(new CustomEvent("odr-sheet-select"));
  }

  // Moves the focus to @p position, the pin staying where it is. False where
  // no cell is pinned or the sheet holds none there.
  function select(position) {
    var at = pinnedPosition();
    if (at === null || at.column === null || at.row === null) {
      return false;
    }
    if (cellAt(position.column, position.row) === null) {
      return false;
    }
    if (
      focus !== null &&
      focus.column === position.column &&
      focus.row === position.row
    ) {
      return true;
    }
    focus = { column: position.column, row: position.row };
    paintSelection();
    return true;
  }

  // What is pinned: a cell, or a whole column or row where a header is, the
  // axis that header does not name being null. Null where nothing is pinned.
  function pinnedPosition() {
    if (pinnedCell === null) {
      return null;
    }
    var position = positionOf(pinnedCell);
    if (position !== null) {
      return { column: position.column, row: position.row, cell: pinnedCell };
    }
    return {
      column: pinnedCell.classList.contains("odr-sheet-column-header")
        ? pinnedCell.cellIndex - 1
        : null,
      row: pinnedRow === null ? null : rowOf(pinnedRow),
      cell: pinnedCell,
    };
  }

  // Pins the cell at a position, as a click on it does; null clears the pin.
  // False where the sheet holds no such cell.
  function pinAt(position) {
    if (position === null) {
      pin(-1, null, null);
      return true;
    }
    var cell = cellAt(position.column, position.row);
    if (cell === null) {
      return false;
    }
    pin(rulerColumn(cell), cell.parentElement, cell);
    return true;
  }

  // Nothing a reader would see, so the cell beside it may spill over it.
  function isBlank(cell) {
    return (
      cell.textContent.trim() === "" &&
      cell.querySelector(":not(x-p):not(x-s)") === null
    );
  }

  // A row's cells by position, a covered one answering with the cell covering
  // it. Null past the sheet's last row.
  function rowCells(row) {
    var entry = indexed().rows.get(row);
    if (entry === undefined) {
      return null;
    }
    if (merged) {
      return entry.cells;
    }
    return Array.prototype.slice.call(entry.tr.cells, 1);
  }

  // The row's cells once each, with what `translate_sheet` states about them:
  // `max-width:0` where the column states a width, which is where it also
  // clips, and `nowrap` where the string may run past the cell.
  function rowState(row) {
    var cells = rowCells(row);
    if (cells === null) {
      return null;
    }
    var line = [];
    for (var i = 0; i < cells.length; ++i) {
      if (cells[i] === cells[i - 1]) {
        continue;
      }
      var style = getComputedStyle(cells[i]);
      line.push({
        cell: cells[i],
        blank: isBlank(cells[i]),
        sized: style.maxWidth === "0px",
        // a cell preserves its white space, so the shorthand reads `pre`
        nowrap: (style.textWrapMode || style.whiteSpace) === "nowrap",
      });
    }
    return line;
  }

  // The spill `translate_sheet` measured goes stale the moment a cell fills or
  // empties: its rule again, off the geometry the browser has. Offsets, not
  // rects: blink scales a rect by the body zoom, a `clip-path` is stated under
  // it.
  function reflow(row) {
    var line = rowState(row);
    if (line === null) {
      return false;
    }

    // What each cell sees to its right: the next one showing something, or
    // the column stating no width that stops the spill before one.
    var bound = null;
    var stopped = false;
    for (var i = line.length - 1; i >= 0; --i) {
      line[i].bound = bound;
      line[i].stopped = stopped;
      if (!line[i].blank) {
        bound = line[i].cell;
        stopped = false;
      } else if (!line[i].sized) {
        bound = null;
        stopped = true;
      }
    }

    for (var j = 0; j < line.length; ++j) {
      var entry = line[j];
      if (!entry.sized || !entry.nowrap) {
        continue;
      }
      var spill =
        entry.bound === null
          ? 0
          : entry.bound.offsetLeft -
            entry.cell.offsetLeft -
            entry.cell.offsetWidth;
      entry.cell.style.overflow =
        spill > 0.5 || (entry.bound === null && !entry.stopped)
          ? "visible"
          : "hidden";
      entry.cell.style.clipPath =
        spill > 0.5 ? "inset(0 " + -spill + "px 0 0)" : "none";
    }
    return true;
  }

  // The run a write goes through, so its style survives; the cell itself
  // where it writes its string without one.
  function runOf(cell) {
    var box = boxOf(cell);
    while (
      box !== null &&
      box.childElementCount === 1 &&
      box.firstElementChild.tagName === "X-S"
    ) {
      box = box.firstElementChild;
    }
    return box;
  }

  // The text of a cell as an op states it: a line per block and per break.
  function textOf(node) {
    var text = "";
    var blocks = 0;
    for (var child = node.firstChild; child !== null; child = child.nextSibling) {
      if (child.nodeType === Node.TEXT_NODE) {
        text += child.nodeValue;
      } else if (child.tagName === "BR") {
        text += "\n";
      } else if (child.nodeType === Node.ELEMENT_NODE) {
        if (child.tagName === "X-P" && blocks++ > 0) {
          text += "\n";
        }
        text += textOf(child);
      }
    }
    return text;
  }

  // What the page shows at a position, shaped the way an op states a value.
  function valueAt(column, row) {
    var cell = cellAt(column, row);
    if (cell === null) {
      return null;
    }
    var text = textOf(cell);
    if (text === "") {
      return { type: "empty" };
    }
    if (cell.classList.contains("odr-value-type-float")) {
      // a formatted number states its value, a plain one is its text
      var stated = cell.getAttribute("data-odr-value");
      var number = stated !== null ? Number(stated) : toNumber(text);
      if (!isNaN(number)) {
        return { type: "number", number: number, text: text };
      }
    }
    return { type: "string", text: text };
  }

  // The expression a formula cell computes, as the file spells it. Null for a
  // cell holding none, and for a read-only render, which writes no scaffolding.
  function formulaAt(column, row) {
    var cell = cellAt(column, row);
    return cell === null ? null : cell.getAttribute("data-odr-formula");
  }

  // Shows @p value at a position, as a write leaves the cell, and reflows
  // the row around it.
  function showValue(column, row, value) {
    var cell = cellAt(column, row);
    if (cell === null) {
      return false;
    }
    lower();
    var run = runOf(cell);
    var blocks = cell.querySelectorAll(":scope > x-p");
    if (run === null && blocks.length === cell.childElementCount) {
      // the lines go into the first block, as the file writes them
      for (var i = 1; i < blocks.length; ++i) {
        blocks[i].remove();
      }
      run = runOf(cell);
    }
    if (run === null) {
      return false;
    }
    run.textContent = value.type === "empty" ? "" : value.text;
    cell.classList.toggle("odr-value-type-float", value.type === "number");
    reflow(row);
    return true;
  }

  // What the script beside this one, and a host, ask of the sheet: positions
  // the way an op names them, and the pin. `spreadsheet-editing.md` decision 8.
  odr.sheet = {
    cellAt: cellAt,
    positionOf: positionOf,
    pinned: pinnedPosition,
    pin: pinAt,
    lower: lower,
    valueAt: valueAt,
    formulaAt: formulaAt,
    showValue: showValue,
    reflow: reflow,
    selection: selectionOf,
    select: select,
    selectedCells: selectedCells,
  };

  table.addEventListener("mouseover", function (event) {
    var column = rulerColumn(event.target.closest("td,th"));
    if (column !== hovered) {
      hovered = column;
      paint();
    }
  });

  table.addEventListener("mouseleave", function () {
    hovered = -1;
    paint();
  });

  /// Whether the sheet is in the mode that writes cells. The editor is a
  /// script of its own, and a read-only view carries none of it.
  function editingEnabled() {
    return (
      odr.editing !== undefined &&
      typeof odr.editing.isEnabled === "function" &&
      odr.editing.isEnabled()
    );
  }

  table.addEventListener("click", function (event) {
    // Selecting inside what is raised must not put the cell back.
    if (raisedContent !== null && raisedContent.contains(event.target)) {
      return;
    }

    var cell = event.target.closest("td,th");
    if (cell === null) {
      return;
    }

    if (event.shiftKey && cell.tagName === "TD" && select(positionOf(cell))) {
      return;
    }

    // Clicking what is pinned clears it - but `detail` counts the clicks, and
    // the second of a double click is the reader selecting a word. Clearing
    // the pin under that flickers the border off again. A cell in an edited
    // sheet holds its pin too, because the editor opens over that cell and
    // reads it. A header opens none, so it still clears.
    if (cell === pinnedCell) {
      var takenByEditor = editingEnabled() && cell.tagName === "TD";
      if (event.detail <= 1 && !takenByEditor) {
        pin(-1, null, null);
      }
      return;
    }

    if (cell.classList.contains("odr-sheet-column-header")) {
      pin(rulerColumn(cell), null, cell);
    } else if (cell.classList.contains("odr-sheet-row-header")) {
      pin(-1, cell.parentElement, cell);
    } else if (cell.classList.contains("odr-sheet-corner")) {
      pin(-1, null, null);
    } else {
      pin(rulerColumn(cell), cell.parentElement, cell);
    }
  });

  // A mouse drawn over the cells in the editing mode spans a selection; a
  // touch scrolls, and a read-only sheet leaves the text to select.
  var dragFrom = null;
  table.addEventListener("pointerdown", function (event) {
    var cell = event.target.closest("td");
    dragFrom =
      event.pointerType === "mouse" &&
      event.button === 0 &&
      !event.shiftKey &&
      cell !== null &&
      editingEnabled()
        ? cell
        : null;
  });
  table.addEventListener("pointermove", function (event) {
    if (dragFrom === null || (event.buttons & 1) === 0) {
      dragFrom = null;
      return;
    }
    var cell = event.target.closest("td");
    if (cell === null || (cell === dragFrom && focus === null)) {
      return;
    }
    if (pinnedCell !== dragFrom) {
      pin(rulerColumn(dragFrom), dragFrom.parentElement, dragFrom);
    }
    event.preventDefault();
    select(positionOf(cell));
  });
  document.addEventListener("pointerup", function () {
    dragFrom = null;
  });

  // The canvas around the sheet included.
  document.addEventListener("click", function (event) {
    if (event.target.closest(".odr-sheet") === null) {
      pin(-1, null, null);
    }
  });

  // Navigation, not editing: a read-only sheet has a pin to clear.
  if (odr.takesKeys("navigation")) {
    document.addEventListener("keydown", function (event) {
      if (event.key === "Escape") {
        pin(-1, null, null);
      }
    });
  }


  var body = table.tBodies[0];
  var original = null;
  var sortedColumn = -1;
  var sortedDirection = 0;

  // Only the rendered text is in the markup, not the number behind it. The last
  // separator is the decimal one, which settles 1,234.56 against 1.234,56.
  function toNumber(text) {
    var cleaned = text.replace(/[^0-9,.eE+-]/g, "");
    if (cleaned.lastIndexOf(",") > cleaned.lastIndexOf(".")) {
      cleaned = cleaned.replace(/\./g, "").replace(",", ".");
    } else {
      cleaned = cleaned.replace(/,/g, "");
    }
    var value = parseFloat(cleaned);
    return isFinite(value) ? value : NaN;
  }

  // Numbers, then text, then blanks: no column is forced into one kind.
  var NUMBER = 0;
  var TEXT = 1;
  var BLANK = 2;

  function keyOf(row, index) {
    var cell = row.children[index];
    var text = cell === undefined ? "" : cell.textContent.trim();
    if (text === "") {
      return { rank: BLANK, value: 0 };
    }
    if (cell.classList.contains("odr-value-type-float")) {
      var value = toNumber(text);
      if (!isNaN(value)) {
        return { rank: NUMBER, value: value };
      }
    }
    return { rank: TEXT, value: text };
  }

  function reorder(rows) {
    var fragment = document.createDocumentFragment();
    for (var i = 0; i < rows.length; ++i) {
      fragment.appendChild(rows[i]);
    }
    body.appendChild(fragment);
  }

  function sortBy(index, direction) {
    if (original === null) {
      original = Array.prototype.slice.call(body.rows);
    }
    if (direction === 0) {
      reorder(original);
      return;
    }

    var rows = Array.prototype.slice.call(body.rows);
    var keys = new Map();
    for (var i = 0; i < rows.length; ++i) {
      keys.set(rows[i], keyOf(rows[i], index));
    }

    // A blank is an absent value, not the smallest one, so it stays last either
    // way round. The stable sort keeps the document's order for ties.
    rows.sort(function (a, b) {
      var x = keys.get(a);
      var y = keys.get(b);
      if (x.rank === BLANK || y.rank === BLANK) {
        return x.rank === y.rank ? 0 : x.rank === BLANK ? 1 : -1;
      }
      if (x.rank !== y.rank) {
        return (x.rank - y.rank) * direction;
      }
      var result =
        x.rank === NUMBER
          ? x.value - y.value
          : x.value.localeCompare(y.value, undefined, { numeric: true });
      return result * direction;
    });
    reorder(rows);
  }

  // A `rowspan` would reach into a row no longer beneath it and a `colspan`
  // breaks the column index, so a merged sheet gets no sort control.
  if (!merged) {
    var headers = table.tHead.rows[0].children;
    for (var column = 1; column < headers.length; ++column) {
      var control = document.createElement("span");
      control.className = "odr-sheet-sort";
      control.setAttribute("title", "sort by column " + headers[column].textContent);
      headers[column].appendChild(control);
    }

    table.addEventListener(
      "click",
      function (event) {
        var control = event.target.closest(".odr-sheet-sort");
        if (control === null) {
          return;
        }
        // The header itself pins the column; only this control sorts it.
        event.stopPropagation();

        var index = control.parentElement.cellIndex;
        var direction =
          index !== sortedColumn ? 1 : sortedDirection === 1 ? -1 : 0;

        sortBy(index, direction);

        control.classList.remove("odr-sheet-sort-asc", "odr-sheet-sort-desc");
        if (direction === 1) {
          control.classList.add("odr-sheet-sort-asc");
        } else if (direction === -1) {
          control.classList.add("odr-sheet-sort-desc");
        }
        if (sortedColumn !== index && sortedColumn >= 0) {
          headers[sortedColumn]
            .querySelector(".odr-sheet-sort")
            .classList.remove("odr-sheet-sort-asc", "odr-sheet-sort-desc");
        }

        sortedColumn = direction === 0 ? -1 : index;
        sortedDirection = direction;
      },
      true
    );
  }
})();
