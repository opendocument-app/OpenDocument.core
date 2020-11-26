#include <html/Html.h>
#include <html/HtmlTranslation.h>
#include <fstream>
#include <odr/Config.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>
#include <common/Document.h>

namespace odr::html {

namespace {
void translateElement(Element element, std::ostream &out,
                      const Config &config);

void translateGeneration(ElementSiblingRange siblings,
                         std::ostream &out, const Config &config) {
  for (auto &&e : siblings) {
    translateElement(e, out, config);
  }
}

void translateElement(Element element, std::ostream &out,
                      const Config &config) {
  if (element.type() == ElementType::UNKNOWN) {
    translateGeneration(element.children(), out, config);
  } else if (element.type() == ElementType::TEXT) {
    out << Html::escapeText(element.text().string());
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

void translateText(const common::TextDocument &document, std::ostream &out,
                   const Config &config) {
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

void HtmlTranslation::translate(const common::Document &document,
                                const std::string &path, const Config &config) {
  std::ofstream out(path);
  if (!out.is_open())
    return; // TODO throw

  out << Html::doctype();
  out << "<html><head>";
  out << Html::defaultHeaders();
  out << "<style>";
  // TODO translate style
  out << "</style>";
  out << "</head>";

  out << "<body " << Html::bodyAttributes(config) << ">";

  if (auto textDocument = dynamic_cast<const common::TextDocument *>(&document);
      textDocument) {
    translateText(*textDocument, out, config);
  }

  out << "</body>";

  out << "<script>";
  out << Html::defaultScript();
  out << "</script>";
  out << "</html>";

  // TODO throw unknown document
}

void HtmlTranslation::edit(const common::Document &document,
                           const std::string &diff) {}

} // namespace odr::common
