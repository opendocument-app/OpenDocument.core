#include <common/Document.h>
#include <common/Html.h>
#include <fstream>
#include <odr/Document.h>
#include <odr/DocumentElements.h>
#include <odr/Html.h>
#include <nlohmann/json.hpp>

namespace odr {

namespace {
void translateElement(Element element, std::ostream &out,
                      const HtmlConfig &config);

void translateGeneration(ElementSiblingRange siblings, std::ostream &out,
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
    out << "<p>";
    translateGeneration(element.children(), out, config);
    out << "</p>";
  } else if (element.type() == ElementType::SPAN) {
    out << "<span>";
    translateGeneration(element.children(), out, config);
    out << "</span>";
  } else {
    // TODO log
  }
}

void translateText(TextDocument document, std::ostream &out,
                   const HtmlConfig &config) {
  // TODO out-source css
  const auto pageProperties = document.pageProperties();
  const std::string style = "width:" + pageProperties.width +
                            ";margin-top:" + pageProperties.marginTop +
                            ";margin-left:" + pageProperties.marginLeft +
                            ";margin-bottom:" + pageProperties.marginBottom +
                            ";margin-right:" + pageProperties.marginRight + ";";

  out << R"(<div id="odr-body" style=")" + style + "\">";
  translateGeneration(document.content(), out, config);
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
  // TODO translate style
  out << "</style>";
  out << "</head>";

  out << "<body " << common::Html::bodyAttributes(config) << ">";

  if (document.documentType() == DocumentType::TEXT) {
    translateText(document.textDocument(), out, config);
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
