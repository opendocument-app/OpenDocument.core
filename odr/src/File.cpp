#include <odr/File.h>

namespace odr {

namespace {
std::string typeToString(const FileType type) {
  switch (type) {
  case FileType::ZIP:
    return "zip";
  case FileType::COMPOUND_FILE_BINARY_FORMAT:
    return "cfb";
  case FileType::OPENDOCUMENT_TEXT:
    return "odt";
  case FileType::OPENDOCUMENT_PRESENTATION:
    return "odp";
  case FileType::OPENDOCUMENT_SPREADSHEET:
    return "ods";
  case FileType::OPENDOCUMENT_GRAPHICS:
    return "odg";
  case FileType::OFFICE_OPEN_XML_DOCUMENT:
    return "docx";
  case FileType::OFFICE_OPEN_XML_PRESENTATION:
    return "pptx";
  case FileType::OFFICE_OPEN_XML_WORKBOOK:
    return "xlsx";
  case FileType::LEGACY_WORD_DOCUMENT:
    return "doc";
  case FileType::LEGACY_POWERPOINT_PRESENTATION:
    return "ppt";
  case FileType::LEGACY_EXCEL_WORKSHEETS:
    return "xls";
  default:
    return "unnamed";
  }
}
} // namespace

FileType FileMeta::typeByExtension(const std::string &extension) noexcept {
  if (extension == "zip")
    return FileType::ZIP;
  if (extension == "cfb")
    return FileType::COMPOUND_FILE_BINARY_FORMAT;
  if (extension == "odt" || extension == "sxw")
    return FileType::OPENDOCUMENT_TEXT;
  if (extension == "odp" || extension == "sxi")
    return FileType::OPENDOCUMENT_PRESENTATION;
  if (extension == "ods" || extension == "sxc")
    return FileType::OPENDOCUMENT_SPREADSHEET;
  if (extension == "odg" || extension == "sxd")
    return FileType::OPENDOCUMENT_GRAPHICS;
  if (extension == "docx")
    return FileType::OFFICE_OPEN_XML_DOCUMENT;
  if (extension == "pptx")
    return FileType::OFFICE_OPEN_XML_PRESENTATION;
  if (extension == "xlsx")
    return FileType::OFFICE_OPEN_XML_WORKBOOK;
  if (extension == "doc")
    return FileType::LEGACY_WORD_DOCUMENT;
  if (extension == "ppt")
    return FileType::LEGACY_POWERPOINT_PRESENTATION;
  if (extension == "xls")
    return FileType::LEGACY_EXCEL_WORKSHEETS;

  return FileType::UNKNOWN;
}

std::string FileMeta::typeAsString() const noexcept {
  return typeToString(type);
}

}
