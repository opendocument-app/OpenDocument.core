// The plain-text editor, which attaches to `odr.editing` like every other
// format's. The lines sit in chunks, and the first line of each states the
// number it counts from. See `docs/design/txt-editing.md`.
(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});

  function TextEditor(textBody) {
    this.textBody = textBody;
    this.past = [];
    this.future = [];

    var self = this;

    this.textBody.addEventListener("beforeinput", function (event) {
      event.preventDefault();

      if (!odr.editing.isEnabled()) {
        odr.editing.refuse("readOnly", null);
        return;
      }

      if (event.inputType === "historyUndo") {
        self.undo();
      } else if (event.inputType === "historyRedo") {
        self.redo();
      } else if (event.inputType === "insertText") {
        self.insertTextAction(event.data);
      } else if (event.inputType === "insertParagraph") {
        self.insertTextAction("\n");
      } else if (event.inputType === "deleteContentBackward") {
        self.removeTextAction("backward");
      } else if (event.inputType === "deleteContentForward") {
        self.removeTextAction("forward");
      } else {
        // a closed list, as the document editor's is: what we cannot record
        // is refused rather than swallowed
        odr.editing.refuse("unsupportedEdit", null);
      }
    });

    this.textBody.addEventListener("paste", function (event) {
      event.preventDefault();
      if (!odr.editing.isEnabled()) {
        odr.editing.refuse("readOnly", null);
        return;
      }
      self.insertTextAction(event.clipboardData.getData("text/plain"));
    });

    this.textBody.addEventListener("drop", function (event) {
      event.preventDefault();
    });

    this.textBody.addEventListener("dragover", function (event) {
      event.preventDefault();
    });
  }

  TextEditor.prototype.isLine = function (node) {
    return node.parentNode !== null && node.parentNode.parentNode === this.textBody;
  };

  TextEditor.prototype.nextLine = function (line) {
    if (line.nextElementSibling !== null) {
      return line.nextElementSibling;
    }
    var chunk = line.parentNode.nextElementSibling;
    return chunk !== null ? chunk.firstElementChild : null;
  };

  TextEditor.prototype.previousLine = function (line) {
    if (line.previousElementSibling !== null) {
      return line.previousElementSibling;
    }
    var chunk = line.parentNode.previousElementSibling;
    return chunk !== null ? chunk.lastElementChild : null;
  };

  // Ignore formatting whitespace nodes when indexing lines.
  // Measure offsets from the line ancestor, including nested search marks.
  TextEditor.prototype.getPosition = function (container, offset) {
    var line = container;
    while (line !== null && !this.isLine(line)) {
      line = line.parentNode;
    }
    if (line === null) {
      return { line: -1, offset: offset };
    }
    var lineNr = Array.prototype.indexOf.call(line.parentNode.children, line);
    for (
      var chunk = line.parentNode.previousElementSibling;
      chunk !== null;
      chunk = chunk.previousElementSibling
    ) {
      lineNr += chunk.childElementCount;
    }
    var range = document.createRange();
    range.selectNodeContents(line);
    range.setEnd(container, offset);
    return { line: lineNr, offset: range.toString().length };
  };

  TextEditor.prototype.getLine = function (lineNr) {
    var chunks = this.textBody.children;
    for (var i = 0; i < chunks.length; ++i) {
      if (lineNr < chunks[i].childElementCount) {
        return chunks[i].children[lineNr];
      }
      lineNr -= chunks[i].childElementCount;
    }
    return null;
  };

  /// Where each chunk counts from, and the digits of the gutter, after an
  /// edit added or removed lines. A chunk with no line left goes, because it
  /// would keep its height.
  TextEditor.prototype.renumber = function () {
    var start = 0;
    for (var chunk = this.textBody.firstElementChild; chunk !== null; ) {
      var next = chunk.nextElementSibling;
      if (chunk.childElementCount === 0) {
        this.textBody.removeChild(chunk);
      } else {
        chunk.firstElementChild.style.counterReset = "odr-line " + start;
        start += chunk.childElementCount;
      }
      chunk = next;
    }
    this.textBody.parentNode.style.setProperty(
      "--odr-text-digits",
      String(start).length
    );
  };

  TextEditor.prototype.getLineText = function (line) {
    return line.textContent;
  };

  TextEditor.prototype.setLineText = function (line, text) {
    line.textContent = text;
    if (text === "") {
      line.appendChild(document.createElement("br"));
    }
  };

  // Counts a line break as one character.
  TextEditor.prototype.movePosition = function (position, delta) {
    var remainingDelta = Math.abs(delta);
    var sign = delta >= 0 ? 1 : -1;

    var lineNr = position.line;
    var offset = position.offset;
    var line = this.getLine(lineNr);
    var lineLength = this.getLineText(line).length;

    while (true) {
      var remaining = sign > 0 ? lineLength - offset : offset;
      var step = Math.min(remaining, remainingDelta);
      offset += sign * step;
      remainingDelta -= step;
      if (remainingDelta === 0) {
        break;
      }

      line = sign > 0 ? this.nextLine(line) : this.previousLine(line);
      if (line === null) {
        break;
      }
      lineLength = this.getLineText(line).length;
      lineNr += sign;
      offset = sign > 0 ? 0 : lineLength;
      remainingDelta -= 1;
    }

    return { line: lineNr, offset: offset };
  };

  TextEditor.prototype.getText = function (from, to) {
    var result = "";
    var line = this.getLine(from.line);
    for (var lineNr = from.line; lineNr <= to.line; ++lineNr) {
      if (lineNr > from.line) {
        result += "\n";
        line = this.nextLine(line);
      }
      var lineText = this.getLineText(line);
      if (from.line === to.line) {
        result += lineText.slice(from.offset, to.offset);
      } else if (lineNr === from.line) {
        result += lineText.slice(from.offset);
      } else if (lineNr === to.line) {
        result += lineText.slice(0, to.offset);
      } else {
        result += lineText;
      }
    }
    return result;
  };

  TextEditor.prototype.insertText = function (position, text) {
    var textLines = text.split("\n");

    var line = this.getLine(position.line);
    var originalText = this.getLineText(line);

    if (textLines.length === 1) {
      this.setLineText(
        line,
        originalText.slice(0, position.offset) +
          textLines[0] +
          originalText.slice(position.offset)
      );
      return {
        line: position.line,
        offset: position.offset + textLines[0].length,
      };
    }

    for (var i = 0; i < textLines.length; ++i) {
      if (i > 0) {
        line.parentNode.insertBefore(
          document.createElement("div"),
          line.nextElementSibling
        );
        line = line.nextElementSibling;
      }

      if (i === 0) {
        this.setLineText(line, originalText.slice(0, position.offset) + textLines[i]);
      } else if (i === textLines.length - 1) {
        this.setLineText(line, textLines[i] + originalText.slice(position.offset));
      } else {
        this.setLineText(line, textLines[i]);
      }
    }

    this.renumber();
    return {
      line: position.line + textLines.length - 1,
      offset: textLines[textLines.length - 1].length,
    };
  };

  TextEditor.prototype.removeText = function (from, to) {
    var firstLine = this.getLine(from.line);
    var lastLine = this.getLine(to.line);

    this.setLineText(
      firstLine,
      this.getLineText(firstLine).slice(0, from.offset) +
        this.getLineText(lastLine).slice(to.offset)
    );

    for (var lineNr = from.line + 1; lineNr <= to.line; ++lineNr) {
      var removed = this.nextLine(firstLine);
      removed.parentNode.removeChild(removed);
    }
    this.renumber();
  };

  TextEditor.prototype.placeCursorAt = function (position) {
    var line = this.getLine(position.line);
    var range = document.createRange();
    range.setStart(line.firstChild, position.offset);
    range.setEnd(line.firstChild, position.offset);
    range.collapse(true);

    var selection = window.getSelection();
    selection.removeAllRanges();
    selection.addRange(range);
  };

  TextEditor.prototype.doChange = function (change) {
    if (change.type === "insertText") {
      this.insertText(change.position, change.text);
    } else if (change.type === "removeText") {
      this.removeText(
        change.position,
        this.movePosition(change.position, change.text.length)
      );
    }
  };

  TextEditor.prototype.invertChange = function (change) {
    return {
      type: change.type === "insertText" ? "removeText" : "insertText",
      text: change.text,
      position: change.position,
    };
  };

  TextEditor.prototype.pushChange = function (change) {
    this.past.push(change);
    this.future = [];
    odr.editing.changed();
  };

  TextEditor.prototype.undo = function () {
    if (this.past.length === 0) {
      return false;
    }
    var change = this.past.pop();
    this.future.push(change);
    this.doChange(this.invertChange(change));
    odr.editing.changed();
    return true;
  };

  TextEditor.prototype.redo = function () {
    if (this.future.length === 0) {
      return false;
    }
    var change = this.future.pop();
    this.past.push(change);
    this.doChange(change);
    odr.editing.changed();
    return true;
  };

  /// The file's whole text, which for a plain file is its whole document.
  TextEditor.prototype.text = function () {
    var result = [];
    for (
      var line = this.getLine(0);
      line !== null;
      line = this.nextLine(line)
    ) {
      result.push(this.getLineText(line));
    }
    return result.join("\n");
  };

  TextEditor.prototype.insertTextAction = function (text) {
    var selection = window.getSelection();
    if (selection.rangeCount !== 1) {
      console.log("Multiple selection ranges, not supported");
      return;
    }
    var range = selection.getRangeAt(0);
    var position = this.getPosition(range.startContainer, range.startOffset);

    if (
      range.startContainer !== range.endContainer ||
      range.startOffset !== range.endOffset
    ) {
      this.removeTextAction("backward");
    }

    var newPosition = this.insertText(position, text);
    this.pushChange({ type: "insertText", text: text, position: position });
    this.placeCursorAt(newPosition);
  };

  TextEditor.prototype.adjacentCharacter = function (position, direction) {
    var text = this.getLineText(this.getLine(position.line));
    var next = odr.editing.characterOffset(text, position.offset, direction);
    return next === position.offset
      ? this.movePosition(position, direction)
      : { line: position.line, offset: next };
  };

  TextEditor.prototype.removeTextAction = function (mode) {
    var selection = window.getSelection();
    if (selection.rangeCount !== 1) {
      console.log("Multiple selection ranges, not supported");
      return;
    }
    var range = selection.getRangeAt(0);
    var startPosition = this.getPosition(range.startContainer, range.startOffset);
    var endPosition = this.getPosition(range.endContainer, range.endOffset);
    var isSelected =
      range.startContainer !== range.endContainer ||
      range.startOffset !== range.endOffset;

    var from = isSelected
      ? startPosition
      : mode === "forward"
        ? startPosition
        : this.adjacentCharacter(startPosition, -1);
    var to = isSelected
      ? endPosition
      : mode === "forward"
        ? this.adjacentCharacter(endPosition, 1)
        : endPosition;

    if (from.line === to.line && from.offset === to.offset) {
      console.log("No text to remove");
      return;
    }

    var removedText = this.getText(from, to);
    this.removeText(from, to);
    this.pushChange({
      type: "removeText",
      text: removedText,
      position: from,
    });
    this.placeCursorAt(from);
  };

  var textBody = document.querySelector(".odr-text-body");
  if (!textBody) {
    return;
  }
  var editor = new TextEditor(textBody);

  odr.editing.attach({
    enable: function () {
      textBody.setAttribute("contenteditable", "true");
    },
    disable: function () {
      textBody.removeAttribute("contenteditable");
    },
    /// One operation or none: a plain file's content is the whole of it.
    operations: function () {
      if (editor.past.length === 0) {
        return [];
      }
      return [{ op: "setContent", text: editor.text() }];
    },
    canUndo: function () {
      return editor.past.length > 0;
    },
    canRedo: function () {
      return editor.future.length > 0;
    },
    undo: function () {
      return editor.undo();
    },
    redo: function () {
      return editor.redo();
    },
    committed: function () {
      editor.past = [];
      editor.future = [];
    },
  });
})();
