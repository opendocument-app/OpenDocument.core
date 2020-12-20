#include <access/StreamUtil.h>
#include <common/Document.h>
#include <common/Html.h>
#include <crypto/CryptoUtil.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <odr/Document.h>
#include <odr/DocumentElements.h>
#include <odr/File.h>
#include <odr/Html.h>
#include <sstream>
#include <svm/Svm2Svg.h>

namespace odr {

namespace {
void translateGeneration(ElementRange siblings, std::ostream &out,
                         const HtmlConfig &config);
void translateElement(Element element, std::ostream &out,
                      const HtmlConfig &config);

std::string
translateRectangularProperties(const RectangularProperties &properties,
                               const std::string &prefix) {
  std::string result;
  // TODO use shorthand if all the same
  if (properties.top)
    result += prefix + "-top:" + *properties.top + ";";
  if (properties.bottom)
    result += prefix + "-bottom:" + *properties.bottom + ";";
  if (properties.left)
    result += prefix + "-left:" + *properties.left + ";";
  if (properties.right)
    result += prefix + "-right:" + *properties.right + ";";
  return result;
}

std::string translateParagraphProperties(const ParagraphElement &properties) {
  std::string result;
  if (properties.textAlign())
    result += "text-align:" + *properties.textAlign() + ";";
  result += translateRectangularProperties(properties.margin(), "margin");
  return result;
}

std::string translateTextProperties(const TextProperties &properties) {
  std::string result;
  if (properties.font.font)
    result += "font-family:" + *properties.font.font + ";";
  if (properties.font.size)
    result += "font-size:" + *properties.font.size + ";";
  if (properties.font.weight)
    result += "font-weight:" + *properties.font.weight + ";";
  if (properties.font.style)
    result += "font-style:" + *properties.font.style + ";";
  if (properties.font.color)
    result += "color:" + *properties.font.color + ";";
  if (properties.backgroundColor)
    result += "background-color:" + *properties.backgroundColor + ";";
  return result;
}

std::string translateTableProperties(const TableElement &properties) {
  std::string result;
  if (properties.width())
    result += "width:" + *properties.width() + ";";
  return result;
}

std::string
translateTableColumnProperties(const TableColumnElement &properties) {
  std::string result;
  if (properties.width())
    result += "width:" + *properties.width() + ";";
  return result;
}

std::string translateTableRowProperties(const TableRowElement &properties) {
  std::string result;
  // TODO
  return result;
}

std::string translateTableCellProperties(const TableCellElement &properties) {
  std::string result;
  result += translateRectangularProperties(properties.padding(), "padding");
  result += translateRectangularProperties(properties.border(), "border");
  return result;
}

std::string translateFrameProperties(const FrameElement &properties) {
  std::string result;
  result += "width:" + *properties.width() + ";";
  result += "height:" + *properties.height() + ";";
  result += "z-index:" + *properties.zIndex() + ";";
  return result;
}

std::string translateDrawingProperties(const DrawingElement &properties) {
  std::string result;
  if (properties.strokeWidth())
    result += "stroke-width:" + *properties.strokeWidth() + ";";
  if (properties.strokeColor())
    result += "stroke:" + *properties.strokeColor() + ";";
  if (properties.fillColor())
    result += "fill:" + *properties.fillColor() + ";";
  if (properties.verticalAlign()) {
    if (*properties.verticalAlign() == "middle")
      result += "display:flex;justify-content:center;flex-direction: column;";
    // TODO else log
  }
  return result;
}

std::string translateRectProperties(RectElement element) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + element.x() + ";";
  result += "top:" + element.y() + ";";
  result += "width:" + element.width() + ";";
  result += "height:" + element.height() + ";";
  result += translateDrawingProperties(element);
  return result;
}

std::string translateLineProperties(LineElement element) {
  std::string result;
  result += translateDrawingProperties(element);
  return result;
}

std::string translateCircleProperties(CircleElement element) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + element.x() + ";";
  result += "top:" + element.y() + ";";
  result += "width:" + element.width() + ";";
  result += "height:" + element.height() + ";";
  result += translateDrawingProperties(element);
  return result;
}

std::string optionalStyleAttribute(const std::string &style) {
  if (style.empty())
    return "";
  return " style=\"" + style + "\"";
}

void translateList(ListElement element, std::ostream &out,
                   const HtmlConfig &config) {
  out << "<ul>";
  for (auto &&i : element.children()) {
    out << "<li>";
    translateGeneration(i.children(), out, config);
    out << "</li>";
  }
  out << "</ul>";
}

void translateTable(TableElement element, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<table";
  out << optionalStyleAttribute(translateTableProperties(element));
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";

  for (auto &&col : element.columns()) {
    out << "<col";
    out << optionalStyleAttribute(translateTableColumnProperties(col));
    out << ">";
  }

  for (auto &&row : element.rows()) {
    out << "<tr";
    out << optionalStyleAttribute(translateTableRowProperties(row));
    out << ">";
    for (auto &&cell : row.cells()) {
      out << "<td";
      out << optionalStyleAttribute(translateTableCellProperties(cell));
      out << ">";
      translateGeneration(cell.children(), out, config);
      out << "</td>";
    }
    out << "</tr>";
  }

  out << "</table>";
}

void translateImage(ImageElement element, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<img style=\"width:100%;height:100%\"";
  out << " alt=\"Error: image not found or unsupported\"";
  out << " src=\"";

  if (element.internal()) {
    auto imageFile = element.imageFile();
    auto imageStream = imageFile.data();
    std::string image;

    if (imageFile.fileType() == FileType::STARVIEW_METAFILE) {
      std::ostringstream svgOut;
      svm::Translator::svg(*imageStream, svgOut);
      image = svgOut.str();
      out << "data:image/svg+xml;base64, ";
    } else {
      image = access::StreamUtil::read(*imageStream);
      // TODO hacky - `image/jpg` works for all common image types in chrome
      out << "data:image/jpg;base64, ";
    }

    // TODO stream
    out << crypto::Util::base64Encode(image);
  } else {
    out << element.href();
  }

  out << "\">";
}

void translateFrame(FrameElement element, std::ostream &out,
                    const HtmlConfig &config) {
  out << "<div";
  out << optionalStyleAttribute(translateFrameProperties(element));
  out << ">";

  for (auto &&e : element.children()) {
    if (e.type() == ElementType::IMAGE) {
      translateImage(e.image(), out, config);
    }
  }

  out << "</div>";
}

void translateRect(RectElement element, std::ostream &out,
                   const HtmlConfig &config) {
  out << "<div";
  out << optionalStyleAttribute(translateRectProperties(element));
  out << ">";
  translateGeneration(element.children(), out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)";
  out << "</div>";
}

void translateLine(LineElement element, std::ostream &out,
                   const HtmlConfig &config) {
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible")";
  out << optionalStyleAttribute("z-index:-1;position:absolute;top:0;left:0;" +
                                translateLineProperties(element));
  out << ">";

  out << "<line";
  out << " x1=\"" << element.x1() << "\" y1=\"" << element.y1() << "\"";
  out << " x2=\"" << element.x2() << "\" y2=\"" << element.y2() << "\"";
  out << " />";

  out << "</svg>";
}

void translateCircle(CircleElement element, std::ostream &out,
                     const HtmlConfig &config) {
  out << "<div";
  out << optionalStyleAttribute(translateCircleProperties(element));
  out << ">";
  translateGeneration(element.children(), out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><circle cx="50%" cy="50%" r="50%" /></svg>)";
  out << "</div>";
}

void translateGeneration(ElementRange siblings, std::ostream &out,
                         const HtmlConfig &config) {
  for (auto &&e : siblings) {
    translateElement(e, out, config);
  }
}

void translateElement(Element element, std::ostream &out,
                      const HtmlConfig &config) {
  if (element.type() == ElementType::UNKNOWN) {
    translateGeneration(element.children(), out, config);
  } else if (element.type() == ElementType::TEXT) {
    // TODO handle whitespace collapse
    out << common::Html::escapeText(element.text().string());
  } else if (element.type() == ElementType::LINE_BREAK) {
    out << "<br>";
  } else if (element.type() == ElementType::PARAGRAPH) {
    out << "<p";
    out << optionalStyleAttribute(
        translateParagraphProperties(element.paragraph()));
    out << ">";
    out << "<span";
    out << optionalStyleAttribute(
        translateTextProperties(element.paragraph().textProperties()));
    out << ">";
    if (element.firstChild())
      translateGeneration(element.children(), out, config);
    else
      out << "<br>";
    out << "</span>";
    out << "</p>";
  } else if (element.type() == ElementType::SPAN) {
    out << "<span";
    out << optionalStyleAttribute(
        translateTextProperties(element.span().textProperties()));
    out << ">";
    translateGeneration(element.children(), out, config);
    out << "</span>";
  } else if (element.type() == ElementType::LINK) {
    out << "<a";
    out << optionalStyleAttribute(
        translateTextProperties(element.link().textProperties()));
    out << " href=\"";
    out << element.link().href();
    out << "\">";
    translateGeneration(element.children(), out, config);
    out << "</a>";
  } else if (element.type() == ElementType::BOOKMARK) {
    out << "<a id=\"";
    out << element.bookmark().name();
    out << "\"></a>";
  } else if (element.type() == ElementType::LIST) {
    translateList(element.list(), out, config);
  } else if (element.type() == ElementType::TABLE) {
    translateTable(element.table(), out, config);
  } else if (element.type() == ElementType::FRAME) {
    translateFrame(element.frame(), out, config);
  } else if (element.type() == ElementType::RECT) {
    translateRect(element.rect(), out, config);
  } else if (element.type() == ElementType::LINE) {
    translateLine(element.line(), out, config);
  } else if (element.type() == ElementType::CIRCLE) {
    translateCircle(element.circle(), out, config);
  } else {
    // TODO log
  }
}

void translateTextDocument(TextDocument document, std::ostream &out,
                           const HtmlConfig &config) {
  const auto pageProperties = document.pageProperties();

  const std::string outerStyle = "width:" + *pageProperties.width + ";";
  const std::string innerStyle =
      "margin-top:" + *pageProperties.marginTop +
      ";margin-left:" + *pageProperties.marginLeft +
      ";margin-bottom:" + *pageProperties.marginBottom +
      ";margin-right:" + *pageProperties.marginRight + ";";

  out << R"(<div style=")" + outerStyle + "\">";
  out << R"(<div style=")" + innerStyle + "\">";
  translateGeneration(document.root().children(), out, config);
  out << "</div>";
  out << "</div>";
}

void translatePresentation(Presentation document, std::ostream &out,
                           const HtmlConfig &config) {
  for (auto &&slide : document.slides()) {
    const auto pageProperties = slide.pageProperties();

    const std::string outerStyle = "width:" + *pageProperties.width +
                                   ";height:" + *pageProperties.height + ";";
    const std::string innerStyle =
        "margin-top:" + *pageProperties.marginTop +
        ";margin-left:" + *pageProperties.marginLeft +
        ";margin-bottom:" + *pageProperties.marginBottom +
        ";margin-right:" + *pageProperties.marginRight + ";";

    out << R"(<div style=")" + outerStyle + "\">";
    out << R"(<div style=")" + innerStyle + "\">";
    translateGeneration(slide.children(), out, config);
    out << "</div>";
    out << "</div>";
  }
}

void translateSpreadsheet(Spreadsheet document, std::ostream &out,
                          const HtmlConfig &config) {
  for (auto &&child : document.root().children()) {
    const auto sheet = child.sheet();
    translateTable(sheet.table(), out, config);
  }
}

void translateGraphics(Drawing document, std::ostream &out,
                       const HtmlConfig &config) {
  for (auto &&page : document.pages()) {
    const auto pageProperties = page.pageProperties();

    const std::string outerStyle = "width:" + *pageProperties.width +
                                   ";height:" + *pageProperties.height + ";";
    const std::string innerStyle =
        "margin-top:" + *pageProperties.marginTop +
        ";margin-left:" + *pageProperties.marginLeft +
        ";margin-bottom:" + *pageProperties.marginBottom +
        ";margin-right:" + *pageProperties.marginRight + ";";

    out << R"(<div style=")" + outerStyle + "\">";
    out << R"(<div style=")" + innerStyle + "\">";
    translateGeneration(page.children(), out, config);
    out << "</div>";
    out << "</div>";
  }
}
} // namespace

HtmlConfig Html::parseConfig(const std::string &path) {
  HtmlConfig result;

  auto json = nlohmann::json::parse(std::ifstream(path));
  // TODO assign config values

  return result;
}

void Html::translate(Document document, const std::string &documentIdentifier,
                     const std::string &path, const HtmlConfig &config) {
  std::ofstream out(path);
  if (!out.is_open())
    return; // TODO throw

  out << common::Html::doctype();
  out << "<html><head>";
  out << common::Html::defaultHeaders();
  out << "<style>";
  out << common::Html::defaultStyle();
  out << "</style>";
  out << "</head>";

  out << "<body " << common::Html::bodyAttributes(config) << ">";

  if (document.documentType() == DocumentType::TEXT) {
    translateTextDocument(document.textDocument(), out, config);
  } else if (document.documentType() == DocumentType::PRESENTATION) {
    translatePresentation(document.presentation(), out, config);
  } else if (document.documentType() == DocumentType::SPREADSHEET) {
    translateSpreadsheet(document.spreadsheet(), out, config);
  } else if (document.documentType() == DocumentType::GRAPHICS) {
    translateGraphics(document.drawing(), out, config);
  } else {
    // TODO throw?
  }

  out << "</body>";

  out << "<script>";
  out << common::Html::defaultScript();
  out << "</script>";
  out << "</html>";
}

void Html::edit(Document document, const std::string &documentIdentifier,
                const std::string &diff) {}

} // namespace odr
