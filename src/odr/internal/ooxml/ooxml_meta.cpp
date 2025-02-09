#include <odr/internal/ooxml/ooxml_meta.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

#include <unordered_map>

namespace odr::internal::ooxml {

FileMeta parse_file_meta(abstract::ReadableFilesystem &filesystem) {
  static const std::unordered_map<common::Path, FileType> types = {
      {common::Path("word/document.xml"), FileType::office_open_xml_document},
      {common::Path("ppt/presentation.xml"),
       FileType::office_open_xml_presentation},
      {common::Path("xl/workbook.xml"), FileType::office_open_xml_workbook},
  };

  FileMeta result;

  if (filesystem.is_file(common::Path("/EncryptionInfo")) &&
      filesystem.is_file(common::Path("/EncryptedPackage"))) {
    result.type = FileType::office_open_xml_encrypted;
    result.password_encrypted = true;
    return result;
  }

  for (auto &&t : types) {
    if (filesystem.is_file(t.first)) {
      result.type = t.second;
      break;
    }
  }

  if (result.type == FileType::unknown) {
    throw NoOfficeOpenXmlFile();
  }

  return result;
}

} // namespace odr::internal::ooxml
