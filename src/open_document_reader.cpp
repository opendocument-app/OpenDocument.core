#include <odr/open_document_reader.h>

#include <odr/document.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <odr/html.h>

#include <internal/common/path.h>
#include <internal/git_info.h>
#include <internal/open_strategy.h>
#include <internal/project_info.h>
#include <internal/resource.h>

#include <fstream>

namespace odr {

std::string OpenDocumentReader::version() noexcept {
  return internal::project_info::version();
}

std::string OpenDocumentReader::commit() noexcept {
  return internal::git_info::commit();
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
  } else if (extension == "wpd") {
    return FileType::word_perfect;
  } else if (extension == "rtf") {
    return FileType::rich_text_format;
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
  } else if (extension == "txt") {
    return FileType::text_file;
  } else if (extension == "csv") {
    return FileType::comma_separated_values;
  } else if (extension == "json") {
    return FileType::javascript_object_notation;
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
  case FileType::word_perfect:
  case FileType::rich_text_format:
    return FileCategory::document;
  case FileType::portable_network_graphics:
  case FileType::graphics_interchange_format:
  case FileType::jpeg:
  case FileType::bitmap_image_file:
  case FileType::starview_metafile:
    return FileCategory::image;
  case FileType::text_file:
  case FileType::comma_separated_values:
  case FileType::javascript_object_notation:
  case FileType::markdown:
    return FileCategory::text;
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
  case FileType::word_perfect:
    return "wpd";
  case FileType::rich_text_format:
    return "rtf";
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
  case FileType::text_file:
    return "txt";
  case FileType::comma_separated_values:
    return "csv";
  case FileType::javascript_object_notation:
    return "json";
  default:
    return "unnamed";
  }
}

std::vector<FileType> OpenDocumentReader::types(const std::string &path) {
  File file(path);
  return internal::open_strategy::types(file.impl());
}

DecodedFile OpenDocumentReader::open(const std::string &path) {
  return DecodedFile(path);
}

Html OpenDocumentReader::html(const std::string &path, const char *password,
                              const std::string &output_path,
                              const HtmlConfig &config) {
  return html(DecodedFile(path), password, output_path, config);
}

Html OpenDocumentReader::html(const DecodedFile &file, const char *password,
                              const std::string &output_path,
                              const HtmlConfig &config) {
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

void OpenDocumentReader::copy_resources(const std::string &to_path) {
  auto resources = internal::Resources::instance();

  for (auto resource : resources.resources()) {
    auto resource_output_path =
        internal::common::Path(to_path).join(resource.path);
    std::filesystem::create_directories(resource_output_path.parent());
    std::ofstream out(resource_output_path.string(), std::ios::binary);
    out.write(resource.data, resource.size);
  }
}

OpenDocumentReader::OpenDocumentReader() = default;

} // namespace odr
