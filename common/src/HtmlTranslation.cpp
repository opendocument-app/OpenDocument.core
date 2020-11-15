#include <common/GenericDocument.h>
#include <common/Html.h>
#include <common/HtmlTranslation.h>
#include <fstream>
#include <odr/Config.h>

namespace odr::common {

namespace {
void translateText(const GenericTextDocument &document, std::ostream &out,
                   const Config &config) {

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
  out << common::Html::defaultScript();
  out << "</script>";
  out << "</html>";

  // TODO throw unknown document
}

void HtmlTranslation::edit(const GenericDocument &document,
                           const std::string &diff) {}

} // namespace odr::common
