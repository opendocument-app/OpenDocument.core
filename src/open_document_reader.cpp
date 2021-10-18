#include <internal/common/constants.h>
#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <odr/html.h>
#include <odr/open_document_reader.h>

namespace odr {

std::string OpenDocumentReader::version() noexcept {
  return internal::common::constants::version();
}

std::string OpenDocumentReader::commit() noexcept {
  return internal::common::constants::commit();
}

Html OpenDocumentReader::html(const std::string &path, const char *password,
                              const HtmlConfig &config) {
  DecodedFile file(path);

  if (file.file_category() == FileCategory::document) {
    auto document_file = file.document_file();
    if (document_file.password_encrypted()) {
      if ((password == nullptr) || !document_file.decrypt(password)) {
        throw WrongPassword();
      }
    }
    auto document = document_file.document();

    std::vector<HtmlPage> pages;

    html::translate(document, path, config);

    return Html(file.file_type(), config, std::move(pages),
                std::move(document));
  }

  throw UnknownFileType();
}

OpenDocumentReader::OpenDocumentReader() = default;

Html::Html(FileType file_type, HtmlConfig config, std::vector<HtmlPage> pages,
           Document document)
    : m_file_type{file_type}, m_config{std::move(config)},
      m_pages{std::move(pages)}, m_document{std::move(document)} {}

const std::vector<HtmlPage> &Html::pages() const { return m_pages; }

void Html::edit(const char *diff) {
  if (m_document) {
    html::edit(*m_document, diff);
  }
}

void Html::save(const std::string &path) const {
  if (m_document) {
    m_document->save(path);
  }
}

HtmlPage::HtmlPage(std::string name, std::string path)
    : m_name{std::move(name)}, m_path{std::move(path)} {}

const std::string &HtmlPage::name() const { return m_name; }

const std::string &HtmlPage::path() const { return m_path; }

} // namespace odr
