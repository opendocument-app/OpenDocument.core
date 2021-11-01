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
  } else if (extension == "cfb") {
    return FileType::compound_file_binary_format;
  } else if (extension == "odt" || extension == "fodt" || extension == "ott" ||
             extension == "odm") {
    return FileType::opendocument_text;
  } else if (extension == "odp" || extension == "fodp" || extension == "otp") {
    return FileType::opendocument_presentation;
  } else if (extension == "ods" || extension == "fods" || extension == "ots") {
    return FileType::opendocument_spreadsheet;
  } else if (extension == "odg" || extension == "fodg" || extension == "otg") {
    return FileType::opendocument_graphics;
  } else if (extension == "docx") {
    return FileType::office_open_xml_document;
  } else if (extension == "pptx") {
    return FileType::office_open_xml_presentation;
  } else if (extension == "xlsx") {
    return FileType::office_open_xml_workbook;
  } else if (extension == "doc") {
    return FileType::legacy_word_document;
  } else if (extension == "ppt") {
    return FileType::legacy_powerpoint_presentation;
  } else if (extension == "xls") {
    return FileType::legacy_excel_worksheets;
  } else if (extension == "pdf") {
    return FileType::portable_document_format;
  } else if (extension == "png") {
    return FileType::portable_network_graphics;
  } else if (extension == "gif") {
    return FileType::graphics_interchange_format;
  } else if (extension == "jpg" || extension == "jpeg" || extension == "jpe" ||
             extension == "jif" || extension == "jfif" || extension == "jfi") {
    return FileType::jpeg;
  } else if (extension == "bmp" || extension == "dib") {
    return FileType::bitmap_image_file;
  } else if (extension == "svm") {
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
  case FileType::portable_network_graphics:
  case FileType::graphics_interchange_format:
  case FileType::jpeg:
  case FileType::bitmap_image_file:
  case FileType::starview_metafile:
    return FileCategory::image;
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
  case FileType::portable_document_format:
    return "pdf";
  case FileType::portable_network_graphics:
    return "png";
  case FileType::graphics_interchange_format:
    return "gif";
  case FileType::jpeg:
    return "jpg";
  case FileType::bitmap_image_file:
    return "bmp";
  case FileType::starview_metafile:
    return "svm";
  default:
    return "unnamed";
  }
}

Html OpenDocumentReader::html(const std::string &path, const char *password,
                              const std::string &output_path,
                              const HtmlConfig &config) {
  DecodedFile file(path);

  if (file.file_type() == FileType::text_file) {
    return html(file.text_file(), output_path, config);
  } else if (file.file_category() == FileCategory::document) {
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

Html OpenDocumentReader::html(const TextFile &text_file,
                              const std::string &output_path,
                              const HtmlConfig &config) {
  return html::translate(text_file, output_path, config);
}

Html OpenDocumentReader::html(const Document &document,
                              const std::string &output_path,
                              const HtmlConfig &config) {
  return html::translate(document, output_path, config);
}

OpenDocumentReader::OpenDocumentReader() = default;

} // namespace odr
