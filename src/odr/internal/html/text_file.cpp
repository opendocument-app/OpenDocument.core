#include <odr/internal/html/text_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/common/null_stream.hpp>
#include <odr/internal/html/html_service.hpp>
#include <odr/internal/html/html_writer.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <sstream>

namespace odr::internal::html {
namespace {

class HtmlServiceImpl final : public HtmlService {
public:
  HtmlServiceImpl(TextFile text_file, HtmlConfig config,
                  std::shared_ptr<Logger> logger)
      : HtmlService(std::move(config), std::move(logger)),
        m_text_file{std::move(text_file)} {
    m_views.emplace_back(
        std::make_shared<HtmlView>(*this, "text", 0, "text.html"));
  }

  void warmup() const override {}

  [[nodiscard]] const HtmlViews &list_views() const override { return m_views; }

  [[nodiscard]] bool exists(const std::string &path) const override {
    if (path == "text.html") {
      return true;
    }

    return false;
  }

  [[nodiscard]] std::string mimetype(const std::string &path) const override {
    if (path == "text.html") {
      return "text/html";
    }

    throw FileNotFound("Unknown path: " + path);
  }

  void write(const std::string &path, std::ostream &out) const override {
    if (path == "text.html") {
      HtmlWriter writer(out, config());
      write_text(writer);
      return;
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_html(const std::string &path,
                           HtmlWriter &out) const override {
    if (path == "text.html") {
      return write_text(out);
    }

    throw FileNotFound("Unknown path: " + path);
  }

  HtmlResources write_text(HtmlWriter &out) const {
    HtmlResources resources;

    out.write_begin();

    out.write_header_begin();
    out.write_header_charset("UTF-8");
    out.write_header_target("_blank");
    out.write_header_title("odr");
    out.write_header_viewport(
        "width=device-width,initial-scale=1.0,user-scalable=yes");
    out.write_header_style_begin();
    out.write_raw(
        ".odr-text{display:flex;flex-direction:row;font-family:monospace;}");
    out.write_raw(
        ".odr-text-nr{display:flex;flex-direction:column;text-align:"
        "right;vertical-align:top;color:#999999;border-right:solid #999999;}");
    out.write_raw(".odr-text-body{display:flex;flex-direction:column;padding-"
                  "left:5pt;white-space:pre;}");
    out.write_raw(".odr-text-wrap{white-space:break-spaces;word-break:break-"
                  "word;overflow-wrap:anywhere;flex-shrink:1;}");
    out.write_raw("[contenteditable]:focus{outline:none;}");
    out.write_header_style_end();
    out.write_header_end();

    out.write_body_begin();

    out.write_element_begin("div", HtmlElementOptions().set_class("odr-text"));

    out.write_element_begin("div",
                            HtmlElementOptions().set_class("odr-text-nr"));
    std::unique_ptr<std::istream> in = m_text_file.stream();
    for (std::uint32_t line = 1; !in->eof(); ++line) {
      out.write_element_begin("div", HtmlElementOptions().set_inline(true));
      out.out() << line;
      out.write_element_end("div");

      NullStream ss_out;
      util::stream::pipe_line(*in, ss_out, false);
    }
    out.write_element_end("div");

    out.write_element_begin("div",
                            HtmlElementOptions().set_attributes(
                                [&](const HtmlAttributeWriterCallback &clb) {
                                  clb("class", "odr-text-body odr-text-wrap");
                                  if (config().editable) {
                                    clb("contenteditable", "true");
                                  }
                                }));
    in = m_text_file.stream();
    while (!in->eof()) {
      out.write_element_begin("div", HtmlElementOptions().set_inline(true));

      std::ostringstream ss_out;
      util::stream::pipe_line(*in, ss_out, false);
      const std::string &line = ss_out.str();
      if (line.empty()) {
        out.write_element_begin(
            "br", HtmlElementOptions().set_close_type(HtmlCloseType::trailing));
      } else {
        out.out() << ss_out.str();
      }

      out.write_element_end("div");
    }
    out.write_element_end("div");

    out.write_element_end("div");

    out.write_element_begin("script");
    out.out() << R"(
function updateLineNumberHeight() {
    const nrCells = document.querySelectorAll(".odr-text-nr > div");
    const textCells = document.querySelectorAll(".odr-text-body > div");

    for (let i = 0; i < textCells.length; i++) {
            const height = textCells[i].offsetHeight;
            nrCells[i].style.height = height + "px";
    }
}

const textNr = document.querySelector(".odr-text-nr");
const textBody = document.querySelector(".odr-text-body");
const history = [];
const future = [];

const resizeObserver = new ResizeObserver(entries => {
    updateLineNumberHeight();
});
resizeObserver.observe(textBody);

textBody.addEventListener("input", (event) => {
  const nrCells = document.querySelectorAll(".odr-text-nr > div");
  const textCells = document.querySelectorAll(".odr-text-body > div");

  const nrCount = nrCells.length;
  const lineCount = textCells.length;
  if (lineCount > nrCount) {
    for (let i = nrCount + 1; i <= lineCount; i++) {
      const nrCell = document.createElement("div");
      nrCell.textContent = i;
      document.querySelector(".odr-text-nr").appendChild(nrCell);
    }
  } else if (lineCount < nrCount) {
    for (let i = nrCount; i > lineCount; --i) {
      document.querySelector(".odr-text-nr").removeChild(
        document.querySelector(".odr-text-nr").lastChild);
    }
  }

  updateLineNumberHeight();
});

function getPosition(container, offset) {
  const line = container.nodeName === "DIV" ? container : container.parentNode;
  const lines = Array.from(textBody.childNodes);
  const lineIndex = lines.indexOf(line);

  return {
    line: lineIndex,
    offset: offset,
  };
}

function getLine(lineNr) {
  const lines = Array.from(textBody.childNodes);
  return lines[lineNr];
}

function getLineText(line) {
  return line.textContent;
}

function setLineText(line, text) {
  line.textContent = text;
  if (text === "") {
    line.appendChild(document.createElement("br"));
  }
}

function movePosition(position, delta) {
  let absDelta = Math.abs(delta);
  const signDelta = delta >= 0 ? 1 : -1;

  let lineNr = position.line;
  let offset = position.offset;
  let line = getLine(lineNr);
  let lineLength = getLineText(line).length;

  while (true) {
    const remaining = signDelta > 0 ? lineLength - offset : offset;
    const step = Math.min(remaining, absDelta);
    offset += signDelta * step;
    absDelta -= step;
    if (absDelta === 0) {
      break;
    }

    line = signDelta > 0 ? line.nextSibling : line.previousSibling;
    if (line === null) {
      break;
    }
    lineLength = getLineText(line).length;
    lineNr += signDelta;
    offset = signDelta > 0 ? 0 : lineLength;
    absDelta -= 1;
  }

  return { line: lineNr, offset: offset };
}

function getText(from, to) {
  let result = "";

  for (let lineNr = from.line; lineNr <= to.line; ++lineNr) {
    if (lineNr > from.line) {
      result += "\n";
    }

    const line = getLine(lineNr);
    const lineText = getLineText(line);

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
}

function insertText(position, text) {
  const textLines = text.split("\n");

  let line = getLine(position.line);
  const originalText = getLineText(line);

  if (textLines.length === 1) {
    const newText =
      originalText.slice(0, position.offset) +
      textLines[0] +
      originalText.slice(position.offset);
    setLineText(line, newText);
    return { line: position.line, offset: position.offset + textLines[0].length };
  }

  for (let i = 0; i < textLines.length; ++i) {
    if (i > 0) {
      textBody.insertBefore(
        document.createElement("div"),
        line.nextSibling
      );
      line = line.nextSibling;

      textNr.appendChild(document.createElement("div"));
      textNr.lastChild.textContent = Array.from(textBody.childNodes).length + 1;
    }

    if (i === 0) {
      const newText =
        originalText.slice(0, position.offset) +
        textLines[i];
      setLineText(line, newText);
    } else if (i === textLines.length - 1) {
      const newText =
        textLines[i] +
        originalText.slice(position.offset);
      setLineText(line, newText);
    } else {
      setLineText(line, textLines[i]);
    }
  }

  return {
    line: position.line + textLines.length - 1,
    offset: textLines[textLines.length - 1].length,
  };
}

function removeText(from, to) {
  const firstLine = getLine(from.line);
  const lastLine = getLine(to.line);

  const newText =
    getLineText(firstLine).slice(0, from.offset) +
    getLineText(lastLine).slice(to.offset);
  setLineText(firstLine, newText);

  for (let lineNr = from.line + 1; lineNr <= to.line; ++lineNr) {
    textBody.removeChild(firstLine.nextSibling);

    textNr.removeChild(textNr.lastChild);
  }
}

function placeCursorAt(start) {
  const line = getLine(start.line);
  const text = line.firstChild;

  const range = document.createRange();
  const selection = window.getSelection();

  range.setStart(text, start.offset);
  range.setEnd(text, start.offset);
  range.collapse(true);

  selection.removeAllRanges();
  selection.addRange(range);
}

function doChange(change) {
  if (change.type === "insertText") {
    insertText(change.position, change.text);
    return;
  }

  if (change.type === "removeText") {
    const toPosition = movePosition(change.position, change.text.length);
    removeText(change.position, toPosition);
    return;
  }
}

function invertChange(change) {
  if (change.type === "insertText") {
    return {
      type: "removeText",
      text: change.text,
      position: change.position,
    };
  }

  if (change.type === "removeText") {
    return {
      type: "insertText",
      text: change.text,
      position: change.position,
    };
  }
}

function undoChange(change) {
  doChange(invertChange(change));
}

function undo() {
  if (history.length === 0) {
    return;
  }

  const lastChange = history.pop();
  undoChange(lastChange);
  future.push(lastChange);
}

function redo() {
  if (future.length === 0) {
    return;
  }

  const nextChange = future.pop();
  doChange(nextChange);
  history.push(nextChange);
}

function insertTextAction(text) {
  const selection = window.getSelection();
  if (selection.rangeCount !== 1) {
    console.log("Multiple selection ranges, not supported");
    return;
  }
  const range = selection.getRangeAt(0);
  const position = getPosition(range.startContainer, range.startOffset);

  if (range.startContainer !== range.endContainer ||
      range.startOffset !== range.endOffset) {
    removeTextAction("backward");
  }
  const newPosition = insertText(position, text);
  history.push({
    type: "insertText",
    text: text,
    position: position,
  });
  console.log("history", history);

  placeCursorAt(newPosition);
}

function removeTextAction(mode) {
  const selection = window.getSelection();
  if (selection.rangeCount !== 1) {
    console.log("Multiple selection ranges, not supported");
    return;
  }
  const range = selection.getRangeAt(0);
  const startPosition = getPosition(range.startContainer, range.startOffset);
  const endPosition = getPosition(range.endContainer, range.endOffset);
  const isSelected =
      range.startContainer !== range.endContainer ||
      range.startOffset !== range.endOffset;

  const fromPosition = isSelected ? startPosition : (mode === "forward" ? startPosition : movePosition(startPosition, -1));
  const toPosition = isSelected ? endPosition : (mode === "forward" ? movePosition(endPosition, 1): endPosition);

  if (fromPosition.line === toPosition.line &&
      fromPosition.offset === toPosition.offset) {
    console.log("No text to remove");
    return;
  }

  const removedText = getText(fromPosition, toPosition);
  removeText(fromPosition, toPosition);
  history.push({
    type: "removeText",
    text: removedText,
    position: fromPosition,
  });

  placeCursorAt(fromPosition);
}

textBody.addEventListener("beforeinput", (e) => {
  e.preventDefault();

  if (e.inputType === "historyUndo") {
    undo();
    return;
  }
  if (e.inputType === "historyRedo") {
    redo();
    return;
  }

  if (e.inputType === "insertText") {
    insertTextAction(e.data);
    return;
  }
  if (e.inputType === "insertParagraph") {
    insertTextAction("\n");
    return;
  }

  if (e.inputType === "deleteContentBackward") {
    removeTextAction("backward");
    return;
  }
  if (e.inputType === "deleteContentForward") {
    removeTextAction("forward");
    return;
  }
});

textBody.addEventListener("paste", (e) => {
  e.preventDefault();

  const plain = e.clipboardData.getData("text/plain");
  insertTextAction(plain);
});

textBody.addEventListener("drop", e => e.preventDefault());
textBody.addEventListener("dragover", e => e.preventDefault());
)";
    out.write_element_end("script");

    out.write_body_end();

    out.write_end();

    return resources;
  }

protected:
  TextFile m_text_file;

  HtmlViews m_views;
};

} // namespace
} // namespace odr::internal::html

namespace odr::internal {

HtmlService
html::create_text_service(const TextFile &text_file,
                          [[maybe_unused]] const std::string &cache_path,
                          HtmlConfig config, std::shared_ptr<Logger> logger) {
  return odr::HtmlService(std::make_unique<HtmlServiceImpl>(
      text_file, std::move(config), std::move(logger)));
}

} // namespace odr::internal
