#include <odr/odr.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/git_info.hpp>
#include <odr/internal/open_strategy.hpp>
#include <odr/internal/project_info.hpp>

std::string odr::version() noexcept {
  return internal::project_info::version();
}

std::string odr::commit() noexcept { return internal::git_info::commit(); }

odr::FileType odr::type_by_extension(const std::string &extension) noexcept {
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

odr::FileCategory odr::category_by_type(const FileType type) noexcept {
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

std::string odr::type_to_string(const FileType type) noexcept {
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

std::string odr::engine_to_string(const DecoderEngine engine) {
  if (engine == DecoderEngine::odr) {
    return "odr";
  } else if (engine == DecoderEngine::poppler) {
    return "poppler";
  } else if (engine == DecoderEngine::wvware) {
    return "wvware";
  }
  throw UnknownDecoderEngine();
}

odr::DecoderEngine odr::engine_by_name(const std::string &name) {
  if (name == "odr") {
    return DecoderEngine::odr;
  } else if (name == "poppler") {
    return DecoderEngine::poppler;
  } else if (name == "wvware") {
    return DecoderEngine::wvware;
  }
  throw UnknownDecoderEngine();
}

std::vector<odr::FileType> odr::types(const std::string &path) {
  return DecodedFile::types(path);
}

std::vector<odr::DecoderEngine> odr::engines(const std::string &path,
                                             const FileType as) {
  return DecodedFile::engines(path, as);
}

odr::DecodedFile odr::open(const std::string &path) {
  return DecodedFile(path);
}

odr::DecodedFile odr::open(const std::string &path, const FileType as) {
  return DecodedFile(path, as);
}

odr::DecodedFile odr::open(const std::string &path,
                           const DecodePreference &preference) {
  return DecodedFile(path, preference);
}
