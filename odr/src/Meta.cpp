#include <odr/Meta.h>

namespace {
std::string typeToString(const odr::FileType type) {
  switch (type) {
  case odr::FileType::ZIP:
    return "zip";
  case odr::FileType::COMPOUND_FILE_BINARY_FORMAT:
    return "cfb";
  case odr::FileType::OPENDOCUMENT_TEXT:
    return "odt";
  case odr::FileType::OPENDOCUMENT_PRESENTATION:
    return "odp";
  case odr::FileType::OPENDOCUMENT_SPREADSHEET:
    return "ods";
  case odr::FileType::OPENDOCUMENT_GRAPHICS:
    return "odg";
  case odr::FileType::OFFICE_OPEN_XML_DOCUMENT:
    return "docx";
  case odr::FileType::OFFICE_OPEN_XML_PRESENTATION:
    return "pptx";
  case odr::FileType::OFFICE_OPEN_XML_WORKBOOK:
    return "xlsx";
  case odr::FileType::LEGACY_WORD_DOCUMENT:
    return "doc";
  case odr::FileType::LEGACY_POWERPOINT_PRESENTATION:
    return "ppt";
  case odr::FileType::LEGACY_EXCEL_WORKSHEETS:
    return "xls";
  default:
    return "unnamed";
  }
}
} // namespace

namespace odr {

std::string FileMeta::typeAsString() const noexcept {
  return typeToString(type);
}

} // namespace odr
