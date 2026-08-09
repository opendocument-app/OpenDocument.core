#include <odr/internal/html/frontend.hpp>

#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/html/common.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>

namespace odr::internal::html {

namespace {

constexpr const char *document_css = R"css(
*{margin:0;position:relative}
body{padding:5px}
x-p{display:block;font-size:0}
x-s{display:inline}
.odr-background{padding:0;background:#525659}
/* The page column, sized to the widest page so pages of differing width centre
   against each other, not against the viewport. The page's side margin is part
   of that width, so fitting the document to a phone screen leaves a gutter
   instead of going edge to edge. */
.odr-pages{display:flex;flex-direction:column;align-items:center;gap:16px;padding:16px 0;width:max-content;min-width:100%}
.odr-page-outer{display:flex;margin:0 16px;background:#fff;box-shadow:0 1px 4px rgba(0,0,0,.5);z-index:-1000}
mark{background:#ff0}
mark.current{background:orange}
)css";

constexpr const char *spreadsheet_css = R"css(
table{border-collapse:collapse;table-layout:fixed}
td{vertical-align:bottom;text-overflow:ellipsis;height:inherit}
x-p{font-family:"Arial",serif;font-size:10pt}
td x-p{height:inherit}
.odr-gridlines-soft table td{border-top:1px solid #c0c0c0;border-left:1px solid #c0c0c0}
.odr-gridlines-hard table td{border:1px solid #c0c0c0!important}
table td.odr-value-type-float{text-align:right}
)css";

constexpr const char *text_css = R"css(
.odr-text{display:flex;flex-direction:row;font-family:monospace}
.odr-text-nr{display:flex;flex-direction:column;text-align:right;vertical-align:top;color:#999;border-right:solid #999}
.odr-text-body{display:flex;flex-direction:column;padding-left:5pt;white-space:pre}
.odr-text-wrap{white-space:break-spaces;word-break:break-word;overflow-wrap:anywhere}
[contenteditable]:focus{outline:none}
)css";

constexpr const char *media_css = R"css(
.odr-media{display:flex;align-items:center;justify-content:center;margin:0;min-height:100vh;background:#000}
.odr-media video{max-width:100%;max-height:100vh}
.odr-media audio{width:100%;max-width:40rem;margin:0 1rem}
)css";

constexpr const char *document_js = R"js(
(function () {
  "use strict";

  var odr = (window.odr = window.odr || {});

  odr.onError = function (code, message) {
    console.error("error " + code + " message " + message);
  };

  var errorIllegalEditNewLine = {
    code: 1,
    message: "new line not supported by this document",
  };

  var modified = {};

  odr.generateDiff = function () {
    var result = { modifiedText: {} };
    for (var path in modified) {
      if (Object.prototype.hasOwnProperty.call(modified, path)) {
        result.modifiedText[path] = modified[path].innerText;
      }
    }
    return JSON.stringify(result);
  };

  new MutationObserver(function (mutations) {
    for (var i = 0; i < mutations.length; ++i) {
      if (mutations[i].type !== "characterData") {
        continue;
      }
      // The nearest owner, not the direct parent: a search `<mark>` may sit
      // between the edited text and the element carrying the path.
      var parent = mutations[i].target.parentElement;
      var owner = parent && parent.closest("[data-odr-path]");
      if (owner) {
        modified[owner.getAttribute("data-odr-path")] = owner;
      }
    }
  }).observe(document.body, {
    childList: true,
    subtree: true,
    characterData: true,
  });

  document.addEventListener("keydown", function (event) {
    if (event.key === "Enter") {
      event.preventDefault();
      odr.onError(errorIllegalEditNewLine.code, errorIllegalEditNewLine.message);
    }
  });

  var marks = [];
  var current = -1;
  var keyword = "";

  // Case- and diacritic-folded `text` plus a folded-index to source-index map
  // (with an end sentinel), so a match maps back onto the source string.
  // Folding per character is what keeps that map right when a character folds
  // to none or to several.
  function fold(text) {
    var folded = "";
    var map = [];
    for (var i = 0; i < text.length; ++i) {
      var character = text[i]
        .normalize("NFD")
        .replace(/[\u0300-\u036f]/g, "")
        .toLowerCase();
      for (var j = 0; j < character.length; ++j) {
        map.push(i);
      }
      folded += character;
    }
    map.push(text.length);
    return { text: folded, map: map };
  }

  function textNodes() {
    var walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT, {
      acceptNode: function (node) {
        var name = node.parentNode ? node.parentNode.nodeName : "";
        if (name === "SCRIPT" || name === "STYLE" || name === "MARK") {
          return NodeFilter.FILTER_REJECT;
        }
        return node.nodeValue.length > 0
          ? NodeFilter.FILTER_ACCEPT
          : NodeFilter.FILTER_REJECT;
      },
    });
    var nodes = [];
    while (walker.nextNode()) {
      nodes.push(walker.currentNode);
    }
    return nodes;
  }

  function markNode(node, needle) {
    var found = [];
    while (true) {
      var folded = fold(node.nodeValue);
      var at = folded.text.indexOf(needle);
      if (at === -1) {
        return found;
      }
      var match = node.splitText(folded.map[at]);
      node = match.splitText(folded.map[at + needle.length] - folded.map[at]);
      var mark = document.createElement("mark");
      mark.className = "highlight";
      match.parentNode.replaceChild(mark, match);
      mark.appendChild(match);
      found.push(mark);
    }
  }

  function select(index) {
    if (current >= 0 && marks[current]) {
      marks[current].classList.remove("current");
    }
    current = index;
    marks[current].classList.add("current");
    marks[current].scrollIntoView({ block: "center", inline: "center" });
  }

  function step(delta, next) {
    if (next !== undefined && next !== null && fold(String(next)).text !== keyword) {
      return odr.search(next);
    }
    if (marks.length === 0) {
      return 0;
    }
    select((current + delta + marks.length) % marks.length);
    return marks.length;
  }

  odr.resetSearch = function () {
    for (var i = 0; i < marks.length; ++i) {
      var parent = marks[i].parentNode;
      if (!parent) {
        continue;
      }
      while (marks[i].firstChild) {
        parent.insertBefore(marks[i].firstChild, marks[i]);
      }
      parent.removeChild(marks[i]);
      parent.normalize();
    }
    marks = [];
    current = -1;
    keyword = "";
  };

  // Highlights every occurrence, selects the first and returns the count.
  odr.search = function (text) {
    odr.resetSearch();
    keyword = fold(text === undefined || text === null ? "" : String(text)).text;
    if (keyword === "") {
      return 0;
    }
    var nodes = textNodes();
    for (var i = 0; i < nodes.length; ++i) {
      marks = marks.concat(markNode(nodes[i], keyword));
    }
    if (marks.length > 0) {
      select(0);
    }
    return marks.length;
  };

  // An argument is searched for first unless it is already the highlighted
  // keyword, so a host can drive search and step from the same string.
  odr.searchNext = function (text) {
    return step(1, text);
  };

  odr.searchPrevious = function (text) {
    return step(-1, text);
  };
})();
)js";

/// Every input is applied to the line `<div>`s by hand, so the line numbers
/// stay in step and undo/redo replay changes instead of the browser's history.
constexpr const char *text_js = R"js(
(function () {
  "use strict";

  function TextEditor(textNr, textBody) {
    this.textNr = textNr;
    this.textBody = textBody;
    this.past = [];
    this.future = [];

    var self = this;

    new ResizeObserver(function () {
      self.updateLineNumberHeight();
    }).observe(this.textBody);

    this.textBody.addEventListener("input", function () {
      var nrCount = self.textNr.querySelectorAll("div").length;
      var lineCount = self.textBody.querySelectorAll("div").length;
      for (var i = nrCount + 1; i <= lineCount; ++i) {
        var nrCell = document.createElement("div");
        nrCell.textContent = String(i);
        self.textNr.appendChild(nrCell);
      }
      for (var j = nrCount; j > lineCount; --j) {
        self.textNr.removeChild(self.textNr.lastChild);
      }
      self.updateLineNumberHeight();
    });

    this.textBody.addEventListener("beforeinput", function (event) {
      event.preventDefault();

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
      }
    });

    this.textBody.addEventListener("paste", function (event) {
      event.preventDefault();
      self.insertTextAction(event.clipboardData.getData("text/plain"));
    });

    this.textBody.addEventListener("drop", function (event) {
      event.preventDefault();
    });

    this.textBody.addEventListener("dragover", function (event) {
      event.preventDefault();
    });
  }

  TextEditor.prototype.updateLineNumberHeight = function () {
    var nrCells = this.textNr.querySelectorAll("div");
    var textCells = this.textBody.querySelectorAll("div");
    for (var i = 0; i < textCells.length && i < nrCells.length; ++i) {
      nrCells[i].style.height = textCells[i].offsetHeight + "px";
    }
  };

  // Lines are the element children: formatted output puts a whitespace text
  // node between them, and counting or indexing those as lines is off by as
  // much as a factor of two.
  TextEditor.prototype.getPosition = function (container, offset) {
    var line = container.nodeName === "DIV" ? container : container.parentNode;
    return {
      line: Array.prototype.indexOf.call(this.textBody.children, line),
      offset: offset,
    };
  };

  TextEditor.prototype.getLine = function (lineNr) {
    return this.textBody.children[lineNr];
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

      line = sign > 0 ? line.nextElementSibling : line.previousElementSibling;
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
    for (var lineNr = from.line; lineNr <= to.line; ++lineNr) {
      if (lineNr > from.line) {
        result += "\n";
      }
      var lineText = this.getLineText(this.getLine(lineNr));
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
        this.textBody.insertBefore(
          document.createElement("div"),
          line.nextElementSibling
        );
        line = line.nextElementSibling;

        this.textNr.appendChild(document.createElement("div"));
        // the line is already in, so the count is the number the cell gets
        this.textNr.lastChild.textContent = String(this.textBody.children.length);
      }

      if (i === 0) {
        this.setLineText(line, originalText.slice(0, position.offset) + textLines[i]);
      } else if (i === textLines.length - 1) {
        this.setLineText(line, textLines[i] + originalText.slice(position.offset));
      } else {
        this.setLineText(line, textLines[i]);
      }
    }

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
      this.textBody.removeChild(firstLine.nextElementSibling);
      this.textNr.removeChild(this.textNr.lastChild);
    }
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
  };

  TextEditor.prototype.undo = function () {
    if (this.past.length === 0) {
      return;
    }
    var change = this.past.pop();
    this.future.push(change);
    this.doChange(this.invertChange(change));
  };

  TextEditor.prototype.redo = function () {
    if (this.future.length === 0) {
      return;
    }
    var change = this.future.pop();
    this.past.push(change);
    this.doChange(change);
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
        : this.movePosition(startPosition, -1);
    var to = isSelected
      ? endPosition
      : mode === "forward"
        ? this.movePosition(endPosition, 1)
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

  var textNr = document.querySelector(".odr-text-nr");
  var textBody = document.querySelector(".odr-text-body");
  if (textNr && textBody) {
    new TextEditor(textNr, textBody);
  }
})();
)js";

/// One of the renderer's own stylesheets or scripts — "shipped" in the sense
/// @ref odr::HtmlConfig::embed_shipped_resources means.
struct Asset {
  HtmlResourceType type;
  const char *mime_type;
  const char *name;
  const char *content;
};

constexpr Asset document_css_asset{HtmlResourceType::css, "text/css",
                                   "document.css", document_css};
constexpr Asset spreadsheet_css_asset{HtmlResourceType::css, "text/css",
                                      "spreadsheet.css", spreadsheet_css};
constexpr Asset text_css_asset{HtmlResourceType::css, "text/css", "text.css",
                               text_css};
constexpr Asset media_css_asset{HtmlResourceType::css, "text/css", "media.css",
                                media_css};
constexpr Asset document_js_asset{HtmlResourceType::js, "text/javascript",
                                  "document.js", document_js};
constexpr Asset text_js_asset{HtmlResourceType::js, "text/javascript",
                              "text.js", text_js};

/// Registers @p asset among the view's resources; `nullopt` to embed it.
HtmlResourceLocation locate(const Asset &asset, const WritingState &state) {
  const odr::HtmlResource resource = HtmlResource::create(
      asset.type, asset.mime_type, asset.name, asset.name,
      odr::File::from_memory(asset.content), true, false, true);
  HtmlResourceLocation location =
      state.config().resource_locator(resource, state.config());
  state.resources().emplace_back(resource, location);
  return location;
}

void write_style(const Asset &asset, const WritingState &state) {
  if (const HtmlResourceLocation location = locate(asset, state);
      location.has_value()) {
    state.out().write_header_style(escape_attribute(*location));
    return;
  }

  state.out().write_header_style_begin();
  state.out().out() << asset.content;
  state.out().write_header_style_end();
}

void write_script(const Asset &asset, const WritingState &state) {
  if (const HtmlResourceLocation location = locate(asset, state);
      location.has_value()) {
    state.out().write_script(escape_attribute(*location));
    return;
  }

  state.out().write_script_begin();
  state.out().out() << asset.content;
  state.out().write_script_end();
}

} // namespace

} // namespace odr::internal::html

namespace odr::internal {

void html::write_document_style(const WritingState &state) {
  write_style(document_css_asset, state);
}

void html::write_spreadsheet_style(const WritingState &state) {
  write_style(spreadsheet_css_asset, state);
}

void html::write_text_style(const WritingState &state) {
  write_style(text_css_asset, state);
}

void html::write_media_style(const WritingState &state) {
  write_style(media_css_asset, state);
}

void html::write_document_script(const WritingState &state) {
  write_script(document_js_asset, state);
}

void html::write_text_script(const WritingState &state) {
  write_script(text_js_asset, state);
}

} // namespace odr::internal
