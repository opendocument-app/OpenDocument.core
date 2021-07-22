#include <internal/abstract/filesystem.h>
#include <internal/common/path.h>
#include <internal/ooxml/ooxml_meta.h>
#include <odr/file.h>
#include <unordered_map>

namespace odr::internal::ooxml {

FileMeta parse_file_meta(abstract::ReadableFilesystem &filesystem) {
  static const std::unordered_map<common::Path, FileType> TYPES = {
      {"word/document.xml", FileType::OFFICE_OPEN_XML_DOCUMENT},
      {"ppt/presentation.xml", FileType::OFFICE_OPEN_XML_PRESENTATION},
      {"xl/workbook.xml", FileType::OFFICE_OPEN_XML_WORKBOOK},
  };

  FileMeta result;

  if (filesystem.is_file("/EncryptionInfo") &&
      filesystem.is_file("/EncryptedPackage")) {
    result.type = FileType::OFFICE_OPEN_XML_ENCRYPTED;
    result.password_encrypted = true;
    return result;
  }

  for (auto &&t : TYPES) {
    if (filesystem.is_file(t.first)) {
      result.type = t.second;
      break;
    }
  }

  return result;
}

} // namespace odr::internal::ooxml
