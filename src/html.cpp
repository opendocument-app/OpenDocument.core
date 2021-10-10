#include <fstream>
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

  internal::html::translate_document(document, out, config);
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
