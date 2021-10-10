#include <fstream>
#include <internal/html/common.h>
#include <internal/html/document.h>
#include <nlohmann/json.hpp>
#include <odr/document.h>
#include <odr/document_cursor.h>
#include <odr/document_element.h>
#include <odr/file.h>
#include <odr/html.h>

using namespace odr::internal;

namespace odr {

HtmlConfig html::parse_config(const std::string &path) {
  HtmlConfig result;

  auto json = nlohmann::json::parse(std::ifstream(path));
  // TODO assign config values

  return result;
}

void html::translate(const Document &document, const std::string &path,
                     const HtmlConfig &config) {
  std::ofstream out(path);
  if (!out.is_open()) {
    return; // TODO throw
  }

  out << internal::html::doctype();
  out << "<html><head>";
  out << internal::html::default_headers();
  out << "<style>";
  out << internal::html::default_style();
  if (document.document_type() == DocumentType::spreadsheet) {
    out << internal::html::default_spreadsheet_style();
  }
  out << "</style>";
  out << "</head>";

  out << "<body " << internal::html::body_attributes(config) << ">";

  auto cursor = document.root_element();

  if (document.document_type() == DocumentType::text) {
    internal::html::translate_text_document(cursor, out, config);
  } else if (document.document_type() == DocumentType::presentation) {
    internal::html::translate_presentation(cursor, out, config);
  } else if (document.document_type() == DocumentType::spreadsheet) {
    internal::html::translate_spreadsheet(cursor, out, config);
  } else if (document.document_type() == DocumentType::drawing) {
    internal::html::translate_drawing(cursor, out, config);
  } else {
    // TODO throw?
  }

  out << "<script>";
  out << internal::html::default_script();
  out << "</script>";

  out << "</body>";
  out << "</html>";
}

void html::edit(const Document &document, const std::string &diff) {
  auto json = nlohmann::json::parse(diff);

  for (auto &&i : json["modifiedText"].items()) {
    auto cursor = document.root_element();
    cursor.move(i.key());
    cursor.element().text().set_content(i.value());
  }
}

} // namespace odr
