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

std::optional<Document> OpenDocumentReader::html(const std::string &path,
                                                 const char *password,
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

    html::translate(document, path, config);

    return document;
  }

  throw UnknownFileType();
}

void OpenDocumentReader::edit(Document &document, const char *diff,
                              const std::string &path) {
  html::edit(document, diff);
  document.save(path);
}

OpenDocumentReader::OpenDocumentReader() = default;

} // namespace odr
