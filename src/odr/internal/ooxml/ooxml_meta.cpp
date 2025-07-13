#include <odr/internal/ooxml/ooxml_meta.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

#include <unordered_map>

namespace odr::internal::ooxml {

FileMeta parse_file_meta(abstract::ReadableFilesystem &filesystem) {
  struct TypeInfo {
    FileType file_type;
    DocumentType document_type;
  };

  static const std::unordered_map<AbsPath, TypeInfo> types = {
      {AbsPath("/word/document.xml"),
       {FileType::office_open_xml_document, DocumentType::text}},
      {AbsPath("/ppt/presentation.xml"),
       {FileType::office_open_xml_presentation, DocumentType::presentation}},
      {AbsPath("/xl/workbook.xml"),
       {FileType::office_open_xml_workbook, DocumentType::spreadsheet}},
  };

  FileMeta result;
  result.document_meta = DocumentMeta();

  if (filesystem.is_file(AbsPath("/EncryptionInfo")) &&
      filesystem.is_file(AbsPath("/EncryptedPackage"))) {
    result.type = FileType::office_open_xml_encrypted;
    result.password_encrypted = true;
    return result;
  }

  for (auto &&t : types) {
    if (filesystem.is_file(t.first)) {
      result.type = t.second.file_type;
      result.document_meta->document_type = t.second.document_type;
      break;
    }
  }

  if (result.type == FileType::unknown) {
    throw NoOfficeOpenXmlFile();
  }

  return result;
}

} // namespace odr::internal::ooxml
