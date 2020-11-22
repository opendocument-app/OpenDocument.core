#include <common/Document.h>
#include <common/DocumentElements.h>
#include <common/Html.h>
#include <common/HtmlTranslation.h>
#include <fstream>
#include <odr/Config.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>

namespace odr::common {

namespace {
void translateElement(const Element &element, std::ostream &out,
                      const Config &config);

void translateGeneration(std::shared_ptr<const Element> element,
                         std::ostream &out, const Config &config) {
  for (auto g = element; element; element = element->nextSibling()) {
    translateElement(*g, out, config);
  }
}

void translateElement(const Element &element, std::ostream &out,
                      const Config &config) {
  if (element.type() == ElementType::UNKNOWN) {
    translateGeneration(element.firstChild(), out, config);
  } else if (element.type() == ElementType::TEXT) {
    out << Html::escapeText(dynamic_cast<const TextElement &>(element).text());
  } else if (element.type() == ElementType::LINE_BREAK) {
    out << "<br>";
  } else if (element.type() == ElementType::PARAGRAPH) {
    out << "<p>";
    translateGeneration(element.firstChild(), out, config);
    out << "</p>";
  } else if (element.type() == ElementType::SPAN) {
    out << "<span>";
    translateGeneration(element.firstChild(), out, config);
    out << "</span>";
  } else {
    // TODO log
  }
}

void translateText(const TextDocument &document, std::ostream &out,
                   const Config &config) {
  // TODO out-source css
  const auto pageProperties = document.pageProperties();
  const std::string style = "width:" + pageProperties.width +
                            ";margin-top:" + pageProperties.marginTop +
                            ";margin-left:" + pageProperties.marginLeft +
                            ";margin-bottom:" + pageProperties.marginBottom +
                            ";margin-right:" + pageProperties.marginRight + ";";

  out << R"(<div id="odr-body" style=")" + style + "\">";
  translateGeneration(document.firstContentElement(), out, config);
  out << "</div>";
}
} // namespace

void HtmlTranslation::translate(const Document &document,
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

  if (auto textDocument = dynamic_cast<const TextDocument *>(&document);
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

void HtmlTranslation::edit(const Document &document,
                           const std::string &diff) {}

} // namespace odr::common
