#include <odr/html.hpp>

#include <odr/archive.hpp>
#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/filesystem.hpp>
#include <odr/global_params.hpp>
#include <odr/html_service.hpp>

#include <odr/internal/html/document.hpp>
#include <odr/internal/html/filesystem.hpp>
#include <odr/internal/html/image_file.hpp>
#include <odr/internal/html/pdf2htmlex_wrapper.hpp>
#include <odr/internal/html/pdf_file.hpp>
#include <odr/internal/html/text_file.hpp>
#include <odr/internal/html/wvware_wrapper.hpp>
#include <odr/internal/oldms_wvware/wvware_oldms_file.hpp>
#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>

#include <filesystem>

#include <nlohmann/json.hpp>

using namespace odr::internal;

namespace odr {

HtmlConfig::HtmlConfig() : resource_path{GlobalParams::odr_core_data_path()} {}

Html::Html(HtmlConfig config, std::vector<HtmlPage> pages)
    : m_config{std::move(config)}, m_pages{std::move(pages)} {}

const HtmlConfig &Html::config() { return m_config; }

const std::vector<HtmlPage> &Html::pages() const { return m_pages; }

HtmlPage::HtmlPage(std::string name, std::string path)
    : name{std::move(name)}, path{std::move(path)} {}

HtmlService html::translate(const DecodedFile &decoded_file,
                            const std::string &output_path,
                            const HtmlConfig &config) {
  if (decoded_file.is_text_file()) {
    return translate(decoded_file.text_file(), output_path, config);
  } else if (decoded_file.is_image_file()) {
    return translate(decoded_file.image_file(), output_path, config);
  } else if (decoded_file.is_archive_file()) {
    return translate(decoded_file.archive_file(), output_path, config);
  } else if (decoded_file.is_document_file()) {
    return translate(decoded_file.document_file(), output_path, config);
  } else if (decoded_file.is_pdf_file()) {
    return translate(decoded_file.pdf_file(), output_path, config);
  }

  throw UnsupportedFileType(decoded_file.file_type());
}

HtmlService html::translate(const TextFile &text_file,
                            const std::string &output_path,
                            const HtmlConfig &config) {
  std::filesystem::create_directories(output_path);
  return internal::html::create_text_service(text_file, output_path, config);
}

HtmlService html::translate(const ImageFile &image_file,
                            const std::string &output_path,
                            const HtmlConfig &config) {
  std::filesystem::create_directories(output_path);
  return internal::html::create_image_service(image_file, output_path, config);
}

HtmlService html::translate(const ArchiveFile &archive_file,
                            const std::string &output_path,
                            const HtmlConfig &config) {
  return translate(archive_file.archive(), output_path, config);
}

HtmlService html::translate(const DocumentFile &document_file,
                            const std::string &output_path,
                            const HtmlConfig &config) {
  auto document_file_impl = document_file.impl();

#ifdef ODR_WITH_WVWARE
  if (auto wv_document_file =
          std::dynamic_pointer_cast<internal::WvWareLegacyMicrosoftFile>(
              document_file_impl)) {
    std::filesystem::create_directories(output_path);
    return internal::html::create_wvware_oldms_service(*wv_document_file,
                                                       output_path, config);
  }
#endif

  return translate(document_file.document(), output_path, config);
}

HtmlService html::translate(const PdfFile &pdf_file,
                            const std::string &output_path,
                            const HtmlConfig &config) {
  auto pdf_file_impl = pdf_file.impl();

#ifdef ODR_WITH_PDF2HTMLEX
  if (auto poppler_pdf_file =
          std::dynamic_pointer_cast<internal::PopplerPdfFile>(pdf_file_impl)) {
    std::filesystem::create_directories(output_path);
    return internal::html::create_poppler_pdf_service(*poppler_pdf_file,
                                                      output_path, config);
  }
#endif

  return internal::html::create_pdf_service(pdf_file, output_path, config);
}

HtmlService html::translate(const Archive &archive,
                            const std::string &output_path,
                            const HtmlConfig &config) {
  std::filesystem::create_directories(output_path);
  return internal::html::create_filesystem_service(archive.filesystem(),
                                                   output_path, config);
}

HtmlService html::translate(const Document &document,
                            const std::string &output_path,
                            const HtmlConfig &config) {
  std::filesystem::create_directories(output_path);
  return internal::html::create_document_service(document, output_path, config);
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
