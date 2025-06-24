#include <odr/odr.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

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

std::string odr::file_category_to_string(FileCategory type) noexcept {
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

std::string odr::decoder_engine_to_string(const DecoderEngine engine) {
  if (engine == DecoderEngine::odr) {
    return "odr";
  } else if (engine == DecoderEngine::poppler) {
    return "poppler";
  } else if (engine == DecoderEngine::wvware) {
    return "wvware";
  }
  throw UnknownDecoderEngine();
}

odr::DecoderEngine odr::decoder_engine_by_name(const std::string &name) {
  if (name == "odr") {
    return DecoderEngine::odr;
  } else if (name == "poppler") {
    return DecoderEngine::poppler;
  } else if (name == "wvware") {
    return DecoderEngine::wvware;
  }
  throw UnknownDecoderEngine();
}

std::vector<odr::FileType> odr::list_file_types(const std::string &path) {
  return DecodedFile::list_file_types(path);
}

std::vector<odr::DecoderEngine> odr::list_decoder_engines(const FileType as) {
  return DecodedFile::list_decoder_engines(as);
}

odr::DecodedFile odr::open(const std::string &path, Logger &logger) {
  return DecodedFile(path, logger);
}

odr::DecodedFile odr::open(const std::string &path, const FileType as,
                           Logger &logger) {
  return DecodedFile(path, as, logger);
}

odr::DecodedFile odr::open(const std::string &path,
                           const DecodePreference &preference, Logger &logger) {
  return DecodedFile(path, preference, logger);
}
