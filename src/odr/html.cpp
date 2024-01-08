#include <odr/html.hpp>

#include <odr/archive.hpp>
#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/filesystem.hpp>

#include <odr/internal/html/document.hpp>
#include <odr/internal/html/filesystem.hpp>
#include <odr/internal/html/image_file.hpp>
#include <odr/internal/html/pdf_file.hpp>
#include <odr/internal/html/text_file.hpp>

#include <filesystem>

#include <nlohmann/json.hpp>

using namespace odr::internal;
namespace fs = std::filesystem;

namespace odr {

Html::Html(FileType file_type, HtmlConfig config, std::vector<HtmlPage> pages)
    : m_file_type{file_type}, m_config{std::move(config)}, m_pages{std::move(
                                                               pages)} {}

Html::Html(FileType file_type, HtmlConfig config, std::vector<HtmlPage> pages,
           Document document)
    : m_file_type{file_type}, m_config{std::move(config)},
      m_pages{std::move(pages)}, m_document{std::move(document)} {}

FileType Html::file_type() const { return m_file_type; }

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
    : name{std::move(name)}, path{std::move(path)} {}

Html html::translate(const File &file, const std::string &output_path,
                     const HtmlConfig &config,
                     const PasswordCallback &password_callback) {
  auto decoded_file = DecodedFile(file);

  if (decoded_file.is_document_file()) {
    DocumentFile document_file = decoded_file.document_file();
    if (document_file.password_encrypted()) {
      if (!document_file.decrypt(password_callback())) {
        throw WrongPassword();
      }
    }
  }

  return translate(decoded_file, output_path, config);
}

Html html::translate(const DecodedFile &decoded_file,
                     const std::string &output_path, const HtmlConfig &config) {
  if (decoded_file.is_text_file()) {
    return translate(decoded_file.text_file(), output_path, config);
  } else if (decoded_file.is_image_file()) {
    return translate(decoded_file.image_file(), output_path, config);
  } else if (decoded_file.is_archive_file()) {
    return translate(decoded_file.archive_file().archive(), output_path,
                     config);
  } else if (decoded_file.is_document_file()) {
    return translate(decoded_file.document_file().document(), output_path,
                     config);
  } else if (decoded_file.is_pdf_file()) {
    return translate(decoded_file.pdf_file(), output_path, config);
  }

  throw UnsupportedFileType(decoded_file.file_type());
}

Html html::translate(const TextFile &text_file, const std::string &output_path,
                     const HtmlConfig &config) {
  fs::create_directories(output_path);
  return internal::html::translate_text_file(text_file, output_path, config);
}

Html html::translate(const ImageFile &image_file,
                     const std::string &output_path, const HtmlConfig &config) {
  fs::create_directories(output_path);
  return internal::html::translate_image_file(image_file, output_path, config);
}

Html html::translate(const Archive &archive, const std::string &output_path,
                     const HtmlConfig &config) {
  fs::create_directories(output_path);
  return internal::html::translate_filesystem(
      FileType::unknown, archive.filesystem(), output_path, config);
}

Html html::translate(const Document &document, const std::string &output_path,
                     const HtmlConfig &config) {
  fs::create_directories(output_path);
  return internal::html::translate_document(document, output_path, config);
}

Html html::translate(const PdfFile &pdf_file, const std::string &output_path,
                     const HtmlConfig &config) {
  fs::create_directories(output_path);
  return internal::html::translate_pdf_file(pdf_file, output_path, config);
}

void html::edit(const Document &document, const char *diff) {
  auto json = nlohmann::json::parse(diff);
  for (const auto &[key, value] : json["modifiedText"].items()) {
    auto element =
        DocumentPath::find(document.root_element(), DocumentPath(key));
    element.text().set_content(value);
  }
}

} // namespace odr
