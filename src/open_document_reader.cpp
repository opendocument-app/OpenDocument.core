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

FileType
OpenDocumentReader::type_by_extension(const std::string &extension) noexcept {
  if (extension == "zip") {
    return FileType::zip;
  }
  if (extension == "cfb") {
    return FileType::compound_file_binary_format;
  }
  if (extension == "odt" || extension == "fodt" || extension == "ott" ||
      extension == "odm") {
    return FileType::opendocument_text;
  }
  if (extension == "odp" || extension == "fodp" || extension == "otp") {
    return FileType::opendocument_presentation;
  }
  if (extension == "ods" || extension == "fods" || extension == "ots") {
    return FileType::opendocument_spreadsheet;
  }
  if (extension == "odg" || extension == "fodg" || extension == "otg") {
    return FileType::opendocument_graphics;
  }
  if (extension == "docx") {
    return FileType::office_open_xml_document;
  }
  if (extension == "pptx") {
    return FileType::office_open_xml_presentation;
  }
  if (extension == "xlsx") {
    return FileType::office_open_xml_workbook;
  }
  if (extension == "doc") {
    return FileType::legacy_word_document;
  }
  if (extension == "ppt") {
    return FileType::legacy_powerpoint_presentation;
  }
  if (extension == "xls") {
    return FileType::legacy_excel_worksheets;
  }
  if (extension == "svm") {
    return FileType::starview_metafile;
  }

  return FileType::unknown;
}

FileCategory
OpenDocumentReader::category_by_type(const FileType type) noexcept {
  switch (type) {
  case FileType::zip:
  case FileType::compound_file_binary_format:
    return FileCategory::archive;
  case FileType::opendocument_text:
  case FileType::opendocument_presentation:
  case FileType::opendocument_spreadsheet:
  case FileType::opendocument_graphics:
  case FileType::office_open_xml_document:
  case FileType::office_open_xml_presentation:
  case FileType::office_open_xml_workbook:
  case FileType::legacy_word_document:
  case FileType::legacy_powerpoint_presentation:
  case FileType::legacy_excel_worksheets:
    return FileCategory::document;
  default:
    return FileCategory::unknown;
  }
}

std::string OpenDocumentReader::type_to_string(const FileType type) noexcept {
  switch (type) {
  case FileType::zip:
    return "zip";
  case FileType::compound_file_binary_format:
    return "cfb";
  case FileType::opendocument_text:
    return "odt";
  case FileType::opendocument_presentation:
    return "odp";
  case FileType::opendocument_spreadsheet:
    return "ods";
  case FileType::opendocument_graphics:
    return "odg";
  case FileType::office_open_xml_document:
    return "docx";
  case FileType::office_open_xml_presentation:
    return "pptx";
  case FileType::office_open_xml_workbook:
    return "xlsx";
  case FileType::legacy_word_document:
    return "doc";
  case FileType::legacy_powerpoint_presentation:
    return "ppt";
  case FileType::legacy_excel_worksheets:
    return "xls";
  default:
    return "unnamed";
  }
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

Html OpenDocumentReader::html(const Document &document,
                              const std::string &output_path,
                              const HtmlConfig &config) {
  return html::translate(document, output_path, config);
}

OpenDocumentReader::OpenDocumentReader() = default;

} // namespace odr
