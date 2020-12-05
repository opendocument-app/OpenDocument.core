#include <common/Document.h>
#include <common/Html.h>
#include <fstream>
#include <nlohmann/json.hpp>
#include <odr/Document.h>
#include <odr/DocumentElements.h>
#include <odr/Html.h>

namespace odr {

namespace {
void translateElement(Element element, std::ostream &out,
                      const HtmlConfig &config);
void translateList(ListElement element, std::ostream &out,
                    const HtmlConfig &config);
void translateTable(TableElement element, std::ostream &out,
                    const HtmlConfig &config);

std::string translateRectangularProperties(
    const RectangularProperties &rectangularProperties,
    const std::string &prefix) {
  std::string result;
  if (rectangularProperties.top)
    result += prefix + "top:" + *rectangularProperties.top + ";";
  if (rectangularProperties.bottom)
    result += prefix + "bottom:" + *rectangularProperties.bottom + ";";
  if (rectangularProperties.left)
    result += prefix + "left:" + *rectangularProperties.left + ";";
  if (rectangularProperties.right)
    result += prefix + "right:" + *rectangularProperties.right + ";";
  return result;
}

std::string
translateParagraphProperties(const ParagraphProperties &paragraphProperties) {
  std::string result;
  if (paragraphProperties.textAlign)
    result += "text-align:" + *paragraphProperties.textAlign + ";";
  result +=
      translateRectangularProperties(paragraphProperties.margin, "margin-");
  return result;
}

std::string translateTextProperties(const TextProperties &textProperties) {
  std::string result;
  if (textProperties.font.font)
    result += "font-family:" + *textProperties.font.font + ";";
  if (textProperties.font.size)
    result += "font-size:" + *textProperties.font.size + ";";
  if (textProperties.font.weight)
    result += "font-weight:" + *textProperties.font.weight + ";";
  if (textProperties.font.style)
    result += "font-style:" + *textProperties.font.style + ";";
  if (textProperties.font.color)
    result += "color:" + *textProperties.font.color + ";";
  if (textProperties.backgroundColor)
    result += "background-color:" + *textProperties.backgroundColor + ";";
  return result;
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
    out << common::Html::escapeText(element.text().string());
  } else if (element.type() == ElementType::LINE_BREAK) {
    out << "<br>";
  } else if (element.type() == ElementType::PARAGRAPH) {
    out << "<p style=\"";
    out << translateParagraphProperties(
        element.paragraph().paragraphProperties());
    out << "\">";
    out << "<span style=\"";
    out << translateTextProperties(element.paragraph().textProperties());
    out << "\">";
    if (element.firstChild())
      translateGeneration(element.children(), out, config);
    else
      out << "<br>";
    out << "</span>";
    out << "</p>";
  } else if (element.type() == ElementType::SPAN) {
    out << "<span style=\"";
    out << translateTextProperties(element.span().textProperties());
    out << "\">";
    translateGeneration(element.children(), out, config);
    out << "</span>";
  } else if (element.type() == ElementType::LINK) {
    out << "<a style=\"";
    out << translateTextProperties(element.link().textProperties());
    out << "\" href=\"";
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
  } else {
    // TODO log
  }
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
  out << "<table>";

  for (auto &&col : element.columns()) {
    out << "<col>";
    out << "</col>";
  }

  for (auto &&row : element.rows()) {
    out << "<tr>";
    for (auto &&cell : row.cells()) {
      out << "<td>";
      translateGeneration(cell.children(), out, config);
      out << "</td>";
    }
    out << "</tr>";
  }

  out << "</table>";
}

void translateText(TextDocument document, std::ostream &out,
                   const HtmlConfig &config) {
  const auto pageProperties = document.pageProperties();

  const std::string outerStyle = "width:" + pageProperties.width + ";";
  const std::string innerStyle =
      "margin-top:" + pageProperties.marginTop +
      ";margin-left:" + pageProperties.marginLeft +
      ";margin-bottom:" + pageProperties.marginBottom +
      ";margin-right:" + pageProperties.marginRight + ";";

  out << R"(<div style=")" + outerStyle + "\">";
  out << R"(<div style=")" + innerStyle + "\">";
  translateGeneration(document.content(), out, config);
  out << "</div>";
  out << "</div>";
}
} // namespace

HtmlConfig Html::parseConfig(const std::string &path) {
  HtmlConfig result;

  auto json = nlohmann::json::parse(std::ifstream(path));
  // TODO assign config values

  return result;
}

void Html::translate(Document document, const std::string &path,
                     const HtmlConfig &config) {
  std::ofstream out(path);
  if (!out.is_open())
    return; // TODO throw

  out << common::Html::doctype();
  out << "<html><head>";
  out << common::Html::defaultHeaders();
  out << "<style>";
  out << common::Html::odfDefaultStyle();
  out << "</style>";
  out << "</head>";

  out << "<body " << common::Html::bodyAttributes(config) << ">";

  if (document.documentType() == DocumentType::TEXT) {
    translateText(document.textDocument(), out, config);
  } else {
    // TODO throw?
  }

  out << "</body>";

  out << "<script>";
  out << common::Html::defaultScript();
  out << "</script>";
  out << "</html>";

  // TODO throw unknown document
}

void Html::edit(Document document, const std::string &diff) {}

} // namespace odr
