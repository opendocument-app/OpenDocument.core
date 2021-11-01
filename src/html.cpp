#include <filesystem>
#include <internal/html/document.h>
#include <internal/html/text_file.h>
#include <nlohmann/json.hpp>
#include <odr/document.h>
#include <odr/document_cursor.h>
#include <odr/document_element.h>
#include <odr/html.h>

using namespace odr::internal;
namespace fs = std::filesystem;

namespace odr {

Html::Html(const FileType file_type, HtmlConfig config,
           std::vector<HtmlPage> pages)
    : m_file_type{file_type}, m_config{std::move(config)}, m_pages{std::move(
                                                               pages)} {}

Html::Html(const FileType file_type, HtmlConfig config,
           std::vector<HtmlPage> pages, Document document)
    : m_file_type{file_type}, m_config{std::move(config)},
      m_pages{std::move(pages)}, m_document{std::move(document)} {}

FileType Html::file_type() const { return m_file_type; }

const std::vector<HtmlPage> &Html::pages() const { return m_pages; }

void Html::edit(const char *diff) {
  if (m_document) {
    auto json = nlohmann::json::parse(diff);
    for (auto &&i : json["modifiedText"].items()) {
      auto cursor = m_document->root_element();
      cursor.move(i.key());
      cursor.element().text().set_content(i.value());
    }
  }
}

void Html::save(const std::string &path) const {
  if (m_document) {
    m_document->save(path);
  }
}

HtmlPage::HtmlPage(std::string name, std::string path)
    : name{std::move(name)}, path{std::move(path)} {}

Html html::translate(const TextFile &text_file, const std::string &path,
                     const HtmlConfig &config) {
  return internal::html::translate_text_file(text_file, path, config);
}

Html html::translate(const Document &document, const std::string &path,
                     const HtmlConfig &config) {
  fs::create_directories(path);
  return internal::html::translate_document(document, path, config);
}

void html::edit(const Document &document, const char *diff) {
  auto json = nlohmann::json::parse(diff);

  for (auto &&i : json["modifiedText"].items()) {
    auto cursor = document.root_element();
    cursor.move(i.key());
    cursor.element().text().set_content(i.value());
  }
}

} // namespace odr
