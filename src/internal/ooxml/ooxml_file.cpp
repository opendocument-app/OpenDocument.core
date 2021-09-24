#include <internal/abstract/archive.h>
#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>
#include <internal/common/archive.h>
#include <internal/common/file.h>
#include <internal/ooxml/ooxml_crypto.h>
#include <internal/ooxml/ooxml_file.h>
#include <internal/ooxml/ooxml_meta.h>
#include <internal/ooxml/presentation/ooxml_presentation_document.h>
#include <internal/ooxml/spreadsheet/ooxml_spreadsheet_document.h>
#include <internal/ooxml/text/ooxml_text_document.h>
#include <internal/util/stream_util.h>
#include <internal/zip/zip_archive.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <optional>
#include <utility>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal::ooxml {

OfficeOpenXmlFile::OfficeOpenXmlFile(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem) {
  m_file_meta = parse_file_meta(*filesystem);
  m_filesystem = std::move(filesystem);
}

std::shared_ptr<abstract::File> OfficeOpenXmlFile::file() const noexcept {
  return {};
}

FileType OfficeOpenXmlFile::file_type() const noexcept {
  return m_file_meta.type;
}

FileMeta OfficeOpenXmlFile::file_meta() const noexcept { return m_file_meta; }

DocumentType OfficeOpenXmlFile::document_type() const {
  return m_file_meta.document_meta->document_type;
}

DocumentMeta OfficeOpenXmlFile::document_meta() const {
  return *m_file_meta.document_meta;
}

bool OfficeOpenXmlFile::password_encrypted() const noexcept {
  return m_file_meta.password_encrypted;
}

EncryptionState OfficeOpenXmlFile::encryption_state() const noexcept {
  return m_encryption_state;
}

bool OfficeOpenXmlFile::decrypt(const std::string &password) {
  // TODO throw if not encrypted
  // TODO throw if decrypted
  std::string encryption_info =
      util::stream::read(*m_filesystem->open("/EncryptionInfo")->read());
  // TODO cache Crypto::Util
  crypto::Util util(encryption_info);
  std::string key = util.derive_key(password);
  if (!util.verify(key)) {
    return false;
  }
  std::string encrypted_package =
      util::stream::read(*m_filesystem->open("/EncryptedPackage")->read());
  std::string decrypted_package = util.decrypt(encrypted_package, key);
  auto memory_file =
      std::make_shared<common::MemoryFile>(std::move(decrypted_package));
  auto zip = std::make_unique<common::ArchiveFile<zip::ReadonlyZipArchive>>(
      zip::ReadonlyZipArchive(memory_file));
  m_filesystem = zip->archive()->filesystem();
  m_file_meta = parse_file_meta(*m_filesystem);
  m_encryption_state = EncryptionState::decrypted;
  return true;
}

std::shared_ptr<abstract::Document> OfficeOpenXmlFile::document() const {
  // TODO throw if encrypted
  switch (file_type()) {
  case FileType::office_open_xml_document:
    return std::make_shared<text::Document>(m_filesystem);
  case FileType::office_open_xml_presentation:
    return std::make_shared<presentation::Document>(m_filesystem);
  case FileType::office_open_xml_workbook:
    return std::make_shared<spreadsheet::Document>(m_filesystem);
  default:
    throw UnsupportedOperation();
  }
}

} // namespace odr::internal::ooxml
