#include <internal/common/constants.h>
#include <internal/html/document.h>
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
                              const std::string &output_path,
                              const HtmlConfig &config) {
  DecodedFile file(path);

  if (file.file_category() == FileCategory::document) {
    auto document_file = file.document_file();
    if (document_file.password_encrypted()) {
      if ((password == nullptr) || !document_file.decrypt(password)) {
        throw WrongPassword();
      }
    }
    return html(document_file.document(), output_path, config);
  }

  throw UnknownFileType();
}

Html OpenDocumentReader::html(Document document, const std::string &output_path,
                              const HtmlConfig &config) {
  return internal::html::translate_document(document, output_path, config);
}

OpenDocumentReader::OpenDocumentReader() = default;

} // namespace odr
