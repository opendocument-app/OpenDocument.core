#include <common/GenericDocument.h>
#include <common/Html.h>
#include <common/HtmlTranslation.h>
#include <fstream>
#include <odr/Config.h>

namespace odr::common {

namespace {
void translateElement(const GenericElement &element, std::ostream &out,
                      const Config &config);

void translateGeneration(std::shared_ptr<const GenericElement> element,
                         std::ostream &out, const Config &config) {
  for (auto g = element; element; element = element->nextSibling()) {
    translateElement(*g, out, config);
  }
}

void translateElement(const GenericElement &element, std::ostream &out,
                      const Config &config) {
  if (element.isUnknown()) {
    translateGeneration(element.firstChild(), out, config);
  } else if (element.isText()) {
    out << Html::escapeText(dynamic_cast<const GenericText &>(element).text());
  } else if (element.isLineBreak()) {
    out << "<br>";
  } else if (element.isParagraph()) {
    out << "<p>";
    translateGeneration(element.firstChild(), out, config);
    out << "</p>";
  } else if (element.isSpan()) {
    out << "<span>";
    translateGeneration(element.firstChild(), out, config);
    out << "</span>";
  } else {
    // TODO log
  }
}

void translateText(const GenericTextDocument &document, std::ostream &out,
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

void HtmlTranslation::translate(const GenericDocument &document,
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

  if (auto textDocument = dynamic_cast<const GenericTextDocument *>(&document);
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

void HtmlTranslation::edit(const GenericDocument &document,
                           const std::string &diff) {}

} // namespace odr::common
