#include <odr/odr.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/git_info.hpp>
#include <odr/internal/project_info.hpp>

std::string odr::version() noexcept {
  return internal::project_info::version();
}

std::string odr::commit_hash() noexcept {
  return internal::git_info::commit_hash();
}

bool odr::is_dirty() noexcept { return internal::git_info::is_dirty(); }

bool odr::is_debug() noexcept { return internal::project_info::is_debug(); }

std::string odr::identify() noexcept {
  return (version().empty() ? "unknown version" : version()) +
         (commit_hash().empty() ? "" : " (" + commit_hash() + ")") +
         (is_dirty() ? " [dirty]" : "") + (is_debug() ? " [debug]" : "");
}

odr::FileType
odr::file_type_by_file_extension(const std::string &extension) noexcept {
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
  if (extension == "wpd") {
    return FileType::word_perfect;
  }
  if (extension == "rtf") {
    return FileType::rich_text_format;
  }
  if (extension == "pdf") {
    return FileType::portable_document_format;
  }
  if (extension == "png") {
    return FileType::portable_network_graphics;
  }
  if (extension == "gif") {
    return FileType::graphics_interchange_format;
  }
  if (extension == "jpg" || extension == "jpeg" || extension == "jpe" ||
      extension == "jif" || extension == "jfif" || extension == "jfi") {
    return FileType::jpeg;
  }
  if (extension == "bmp" || extension == "dib") {
    return FileType::bitmap_image_file;
  }
  if (extension == "svm") {
    return FileType::starview_metafile;
  }
  if (extension == "txt") {
    return FileType::text_file;
  }
  if (extension == "csv") {
    return FileType::comma_separated_values;
  }
  if (extension == "json") {
    return FileType::javascript_object_notation;
  }
  return FileType::unknown;
}

odr::FileCategory
odr::file_category_by_file_type(const FileType type) noexcept {
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

odr::DocumentType
odr::document_type_by_file_type(const FileType type) noexcept {
  switch (type) {
  case FileType::opendocument_text:
    return DocumentType::text;
  case FileType::opendocument_presentation:
    return DocumentType::presentation;
  case FileType::opendocument_spreadsheet:
    return DocumentType::spreadsheet;
  case FileType::opendocument_graphics:
    return DocumentType::drawing;
  case FileType::office_open_xml_document:
    return DocumentType::text;
  case FileType::office_open_xml_presentation:
    return DocumentType::presentation;
  case FileType::office_open_xml_workbook:
    return DocumentType::spreadsheet;
  case FileType::legacy_word_document:
    return DocumentType::text;
  case FileType::legacy_powerpoint_presentation:
    return DocumentType::presentation;
  case FileType::legacy_excel_worksheets:
    return DocumentType::spreadsheet;
  default:
    return DocumentType::unknown;
  }
}

std::string odr::file_type_to_string(const FileType type) noexcept {
  switch (type) {
  case FileType::unknown:
    return "unknown";
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

std::string odr::file_category_to_string(const FileCategory type) noexcept {
  switch (type) {
  case FileCategory::unknown:
    return "unknown";
  case FileCategory::archive:
    return "archive";
  case FileCategory::document:
    return "document";
  case FileCategory::image:
    return "image";
  case FileCategory::text:
    return "text";
  default:
    return "unnamed";
  }
}

std::string odr::document_type_to_string(const DocumentType type) noexcept {
  switch (type) {
  case DocumentType::unknown:
    return "unknown";
  case DocumentType::text:
    return "text";
  case DocumentType::presentation:
    return "presentation";
  case DocumentType::spreadsheet:
    return "spreadsheet";
  case DocumentType::drawing:
    return "drawing";
  default:
    return "unnamed";
  }
}

odr::FileType
odr::file_type_by_mimetype(const std::string_view mimetype) noexcept {
  if (mimetype == "application/vnd.oasis.opendocument.text") {
    return FileType::opendocument_text;
  }
  if (mimetype == "application/vnd.oasis.opendocument.presentation") {
    return FileType::opendocument_presentation;
  }
  if (mimetype == "application/vnd.oasis.opendocument.spreadsheet") {
    return FileType::opendocument_spreadsheet;
  }
  if (mimetype == "application/vnd.oasis.opendocument.graphics") {
    return FileType::opendocument_graphics;
  }
  if (mimetype ==
      "application/"
      "vnd.openxmlformats-officedocument.wordprocessingml.document") {
    return FileType::office_open_xml_document;
  }
  if (mimetype ==
      "application/"
      "vnd.openxmlformats-officedocument.presentationml.presentation") {
    return FileType::office_open_xml_presentation;
  }
  if (mimetype ==
      "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet") {
    return FileType::office_open_xml_workbook;
  }
  if (mimetype == "application/msword") {
    return FileType::legacy_word_document;
  }
  if (mimetype == "application/vnd.ms-powerpoint") {
    return FileType::legacy_powerpoint_presentation;
  }
  if (mimetype == "application/vnd.ms-excel") {
    return FileType::legacy_excel_worksheets;
  }
  if (mimetype == "application/zip" ||
      mimetype == "application/x-zip-compressed") {
    return FileType::zip;
  }
  if (mimetype == "application/pdf") {
    return FileType::portable_document_format;
  }
  if (mimetype == "text/plain") {
    return FileType::text_file;
  }
  if (mimetype == "text/csv") {
    return FileType::comma_separated_values;
  }
  if (mimetype == "application/json") {
    return FileType::javascript_object_notation;
  }
  if (mimetype == "text/markdown") {
    return FileType::markdown;
  }
  if (mimetype == "image/png") {
    return FileType::portable_network_graphics;
  }
  if (mimetype == "image/gif") {
    return FileType::graphics_interchange_format;
  }
  if (mimetype == "image/jpeg") {
    return FileType::jpeg;
  }
  if (mimetype == "image/bmp") {
    return FileType::bitmap_image_file;
  }
  return FileType::unknown;
}

std::string_view odr::mimetype_by_file_type(const FileType type) {
  if (type == FileType::opendocument_text) {
    return "application/vnd.oasis.opendocument.text";
  }
  if (type == FileType::opendocument_presentation) {
    return "application/vnd.oasis.opendocument.presentation";
  }
  if (type == FileType::opendocument_spreadsheet) {
    return "application/vnd.oasis.opendocument.spreadsheet";
  }
  if (type == FileType::opendocument_graphics) {
    return "application/vnd.oasis.opendocument.graphics";
  }
  if (type == FileType::office_open_xml_document) {
    return "application/"
           "vnd.openxmlformats-officedocument.wordprocessingml.document";
  }
  if (type == FileType::office_open_xml_presentation) {
    return "application/"
           "vnd.openxmlformats-officedocument.presentationml.presentation";
  }
  if (type == FileType::office_open_xml_workbook) {
    return "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet";
  }
  if (type == FileType::legacy_word_document) {
    return "application/msword";
  }
  if (type == FileType::legacy_powerpoint_presentation) {
    return "application/vnd.ms-powerpoint";
  }
  if (type == FileType::legacy_excel_worksheets) {
    return "application/vnd.ms-excel";
  }
  if (type == FileType::zip) {
    return "application/zip";
  }
  if (type == FileType::portable_document_format) {
    return "application/pdf";
  }
  if (type == FileType::text_file) {
    return "text/plain";
  }
  if (type == FileType::comma_separated_values) {
    return "text/csv";
  }
  if (type == FileType::javascript_object_notation) {
    return "application/json";
  }
  if (type == FileType::markdown) {
    return "text/markdown";
  }
  if (type == FileType::portable_network_graphics) {
    return "image/png";
  }
  if (type == FileType::graphics_interchange_format) {
    return "image/gif";
  }
  if (type == FileType::jpeg) {
    return "image/jpeg";
  }
  if (type == FileType::bitmap_image_file) {
    return "image/bmp";
  }
  throw UnsupportedFileType(type);
}

std::string odr::decoder_engine_to_string(const DecoderEngine engine) {
  if (engine == DecoderEngine::odr) {
    return "odr";
  }
  if (engine == DecoderEngine::poppler) {
    return "poppler";
  }
  if (engine == DecoderEngine::wvware) {
    return "wvware";
  }
  throw UnknownDecoderEngine();
}

odr::DecoderEngine odr::decoder_engine_by_name(const std::string &engine) {
  if (engine == "odr") {
    return DecoderEngine::odr;
  }
  if (engine == "poppler") {
    return DecoderEngine::poppler;
  }
  if (engine == "wvware") {
    return DecoderEngine::wvware;
  }
  throw UnknownDecoderEngine();
}

std::vector<odr::FileType> odr::list_file_types(const std::string &path,
                                                Logger &logger) {
  return DecodedFile::list_file_types(path, logger);
}

std::vector<odr::DecoderEngine> odr::list_decoder_engines(const FileType as) {
  return DecodedFile::list_decoder_engines(as);
}

std::string_view odr::mimetype(const std::string &path, Logger &logger) {
  return DecodedFile::mimetype(path, logger);
}

odr::DecodedFile odr::open(const std::string &path, Logger &logger) {
  return DecodedFile(path, logger);
}

odr::DecodedFile odr::open(const std::string &path, const FileType as,
                           Logger &logger) {
  return {path, as, logger};
}

odr::DecodedFile odr::open(const std::string &path,
                           const DecodePreference &preference, Logger &logger) {
  return {path, preference, logger};
}
