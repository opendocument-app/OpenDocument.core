#include <access/stream_util.h>
#include <common/document.h>
#include <common/html.h>
#include <crypto/crypto_util.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <odr/document.h>
#include <odr/document_elements.h>
#include <odr/document_style.h>
#include <odr/file.h>
#include <odr/html.h>
#include <sstream>
#include <svm/svm_to_svg.h>

namespace odr {

namespace {
void translateGeneration(ElementRange siblings, std::ostream &out,
                         const HtmlConfig &config);
void translateElement(Element element, std::ostream &out,
                      const HtmlConfig &config);

std::string translateParagraphStyle(const ParagraphStyle &style) {
  std::string result;
  if (style.textAlign())
    result += "text-align:" + *style.textAlign() + ";";
  if (style.marginTop())
    result += "margin-top:" + *style.marginTop() + ";";
  if (style.marginBottom())
    result += "margin-bottom:" + *style.marginBottom() + ";";
  if (style.marginLeft())
    result += "margin-left:" + *style.marginLeft() + ";";
  if (style.marginRight())
    result += "margin-right:" + *style.marginRight() + ";";
  return result;
}

std::string translateTextStyle(const TextStyle &style) {
  std::string result;
  if (style.fontName())
    result += "font-family:" + *style.fontName() + ";";
  if (style.fontSize())
    result += "font-size:" + *style.fontSize() + ";";
  if (style.fontWeight())
    result += "font-weight:" + *style.fontWeight() + ";";
  if (style.fontStyle())
    result += "font-style:" + *style.fontStyle() + ";";
  if (style.fontColor())
    result += "color:" + *style.fontColor() + ";";
  if (style.backgroundColor())
    result += "background-color:" + *style.backgroundColor() + ";";
  return result;
}

std::string translateTableStyle(const TableStyle &style) {
  std::string result;
  if (style.width())
    result += "width:" + *style.width() + ";";
  return result;
}

std::string translateTableColumnStyle(const TableColumnStyle &style) {
  std::string result;
  if (style.width())
    result += "width:" + *style.width() + ";";
  return result;
}

std::string translateTableCellStyle(const TableCellStyle &style) {
  std::string result;
  if (style.paddingTop())
    result += "padding-top:" + *style.paddingTop() + ";";
  if (style.paddingBottom())
    result += "padding-bottom:" + *style.paddingBottom() + ";";
  if (style.paddingLeft())
    result += "padding-left:" + *style.paddingLeft() + ";";
  if (style.paddingRight())
    result += "padding-right:" + *style.paddingRight() + ";";
  if (style.borderTop())
    result += "border-top:" + *style.borderTop() + ";";
  if (style.borderBottom())
    result += "border-bottom:" + *style.borderBottom() + ";";
  if (style.borderLeft())
    result += "border-left:" + *style.borderLeft() + ";";
  if (style.borderRight())
    result += "border-right:" + *style.borderRight() + ";";
  return result;
}

std::string translateDrawingStyle(const DrawingStyle &style) {
  std::string result;
  if (style.strokeWidth())
    result += "stroke-width:" + *style.strokeWidth() + ";";
  if (style.strokeColor())
    result += "stroke:" + *style.strokeColor() + ";";
  if (style.fillColor())
    result += "fill:" + *style.fillColor() + ";";
  if (style.verticalAlign()) {
    if (*style.verticalAlign() == "middle")
      result += "display:flex;justify-content:center;flex-direction: column;";
    // TODO else log
  }
  return result;
}

std::string translateFrameProperties(const FrameElement &properties) {
  std::string result;
  result += "width:" + *properties.width() + ";";
  result += "height:" + *properties.height() + ";";
  result += "z-index:" + *properties.zIndex() + ";";
  return result;
}

std::string translateRectProperties(RectElement element) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + element.x() + ";";
  result += "top:" + element.y() + ";";
  result += "width:" + element.width() + ";";
  result += "height:" + element.height() + ";";
  return result;
}

std::string translateCircleProperties(CircleElement element) {
  std::string result;
  result += "position:absolute;";
  result += "left:" + element.x() + ";";
  result += "top:" + element.y() + ";";
  result += "width:" + element.width() + ";";
  result += "height:" + element.height() + ";";
  return result;
}

std::string optionalStyleAttribute(const std::string &style) {
  if (style.empty())
    return "";
  return " style=\"" + style + "\"";
}

void translateParagraph(ParagraphElement element, std::ostream &out,
                        const HtmlConfig &config) {
  out << "<p";
  out << optionalStyleAttribute(
      translateParagraphStyle(element.paragraphStyle()));
  out << ">";
  out << "<span";
  out << optionalStyleAttribute(translateTextStyle(element.textStyle()));
  out << ">";
  if (element.firstChild())
    translateGeneration(element.children(), out, config);
  else
    out << "<br>";
  out << "</span>";
  out << "</p>";
}

void translateSpan(SpanElement element, std::ostream &out,
                   const HtmlConfig &config) {
  out << "<span";
  out << optionalStyleAttribute(translateTextStyle(element.textStyle()));
  out << ">";
  translateGeneration(element.children(), out, config);
  out << "</span>";
}

void translateLink(LinkElement element, std::ostream &out,
                   const HtmlConfig &config) {
  out << "<a";
  out << optionalStyleAttribute(translateTextStyle(element.textStyle()));
  out << " href=\"";
  out << element.href();
  out << "\">";
  translateGeneration(element.children(), out, config);
  out << "</a>";
}

void translateBookmark(BookmarkElement element, std::ostream &out,
                       const HtmlConfig &config) {
  out << "<a id=\"";
  out << element.name();
  out << "\"></a>";
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
  out << optionalStyleAttribute(translateTableStyle(element.tableStyle()));
  out << R"( cellpadding="0" border="0" cellspacing="0")";
  out << ">";

  for (auto &&col : element.columns()) {
    out << "<col";
    out << optionalStyleAttribute(
        translateTableColumnStyle(col.tableColumnStyle()));
    out << ">";
  }

  for (auto &&row : element.rows()) {
    out << "<tr>";
    for (auto &&cell : row.cells()) {
      out << "<td";
      out << optionalStyleAttribute(
          translateTableCellStyle(cell.tableCellStyle()));
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
  out << optionalStyleAttribute(translateRectProperties(element) +
                                translateDrawingStyle(element.drawingStyle()));
  out << ">";
  translateGeneration(element.children(), out, config);
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible" preserveAspectRatio="none" style="z-index:-1;width:inherit;height:inherit;position:absolute;top:0;left:0;padding:inherit;"><rect x="0" y="0" width="100%" height="100%" /></svg>)";
  out << "</div>";
}

void translateLine(LineElement element, std::ostream &out,
                   const HtmlConfig &config) {
  out << R"(<svg xmlns="http://www.w3.org/2000/svg" version="1.1" overflow="visible")";
  out << optionalStyleAttribute("z-index:-1;position:absolute;top:0;left:0;" +
                                translateDrawingStyle(element.drawingStyle()));
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
  out << optionalStyleAttribute(translateCircleProperties(element) +
                                translateDrawingStyle(element.drawingStyle()));
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
  if (element.type() == ElementType::TEXT) {
    // TODO handle whitespace collapse
    out << common::Html::escapeText(element.text().string());
  } else if (element.type() == ElementType::LINE_BREAK) {
    out << "<br>";
  } else if (element.type() == ElementType::PARAGRAPH) {
    translateParagraph(element.paragraph(), out, config);
  } else if (element.type() == ElementType::SPAN) {
    translateSpan(element.span(), out, config);
  } else if (element.type() == ElementType::LINK) {
    translateLink(element.link(), out, config);
  } else if (element.type() == ElementType::BOOKMARK) {
    translateBookmark(element.bookmark(), out, config);
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
  const auto pageStyle = document.pageStyle();

  const std::string outerStyle = "width:" + *pageStyle.width() + ";";
  const std::string innerStyle = "margin-top:" + *pageStyle.marginTop() +
                                 ";margin-left:" + *pageStyle.marginLeft() +
                                 ";margin-bottom:" + *pageStyle.marginBottom() +
                                 ";margin-right:" + *pageStyle.marginRight() +
                                 ";";

  out << R"(<div style=")" + outerStyle + "\">";
  out << R"(<div style=")" + innerStyle + "\">";
  translateGeneration(document.root().children(), out, config);
  out << "</div>";
  out << "</div>";
}

void translatePresentation(Presentation document, std::ostream &out,
                           const HtmlConfig &config) {
  for (auto &&slide : document.slides()) {
    const auto pageStyle = slide.pageStyle();

    const std::string outerStyle =
        "width:" + *pageStyle.width() + ";height:" + *pageStyle.height() + ";";
    const std::string innerStyle =
        "margin-top:" + *pageStyle.marginTop() +
        ";margin-left:" + *pageStyle.marginLeft() +
        ";margin-bottom:" + *pageStyle.marginBottom() +
        ";margin-right:" + *pageStyle.marginRight() + ";";

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
    const auto pageStyle = page.pageStyle();

    const std::string outerStyle =
        "width:" + *pageStyle.width() + ";height:" + *pageStyle.height() + ";";
    const std::string innerStyle =
        "margin-top:" + *pageStyle.marginTop() +
        ";margin-left:" + *pageStyle.marginLeft() +
        ";margin-bottom:" + *pageStyle.marginBottom() +
        ";margin-right:" + *pageStyle.marginRight() + ";";

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
