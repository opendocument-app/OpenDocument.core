#include <odr/internal/ooxml/ooxml_meta.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/path.hpp>

#include <unordered_map>

namespace odr::internal::ooxml {

FileMeta parse_file_meta(const abstract::ReadableFilesystem &filesystem) {
  struct TypeInfo {
    FileType file_type{FileType::unknown};
    DocumentType document_type{DocumentType::unknown};
    std::string_view mimetype;
  };

  static const std::unordered_map<AbsPath, TypeInfo> types = {
      {AbsPath("/word/document.xml"),
       {FileType::office_open_xml_document, DocumentType::text,
        "application/"
        "vnd.openxmlformats-officedocument.wordprocessingml.document"}},
      {AbsPath("/ppt/presentation.xml"),
       {FileType::office_open_xml_presentation, DocumentType::presentation,
        "application/"
        "vnd.openxmlformats-officedocument.presentationml.presentation"}},
      {AbsPath("/xl/workbook.xml"),
       {FileType::office_open_xml_workbook, DocumentType::spreadsheet,
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"}},
  };

  FileMeta result;
  result.document_meta = DocumentMeta();

  if (filesystem.is_file(AbsPath("/EncryptionInfo")) &&
      filesystem.is_file(AbsPath("/EncryptedPackage"))) {
    result.type = FileType::office_open_xml_encrypted;
    result.password_encrypted = true;
    return result;
  }

  for (const auto &[path, info] : types) {
    if (filesystem.is_file(path)) {
      result.type = info.file_type;
      result.mimetype = info.mimetype;
      result.document_meta->document_type = info.document_type;
      break;
    }
  }

  if (result.type == FileType::unknown) {
    throw NoOfficeOpenXmlFile();
  }

  return result;
}

} // namespace odr::internal::ooxml
