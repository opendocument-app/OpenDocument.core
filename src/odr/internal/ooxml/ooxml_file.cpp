#include <odr/internal/ooxml/ooxml_file.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/ooxml/ooxml_crypto.hpp>
#include <odr/internal/ooxml/ooxml_meta.hpp>
#include <odr/internal/ooxml/presentation/ooxml_presentation_document.hpp>
#include <odr/internal/ooxml/spreadsheet/ooxml_spreadsheet_document.hpp>
#include <odr/internal/ooxml/text/ooxml_text_document.hpp>
#include <odr/internal/util/stream_util.hpp>
#include <odr/internal/zip/zip_file.hpp>

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

DecoderEngine OfficeOpenXmlFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

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

std::shared_ptr<abstract::DecodedFile>
OfficeOpenXmlFile::decrypt(const std::string &password) const noexcept {
  if (m_encryption_state != EncryptionState::encrypted) {
    return nullptr;
  }

  std::string encryption_info = util::stream::read(
      *m_filesystem->open(common::Path("/EncryptionInfo"))->stream());
  // TODO cache Crypto::Util
  crypto::Util util(encryption_info);
  std::string key = util.derive_key(password);
  if (!util.verify(key)) {
    return nullptr;
  }

  std::string encrypted_package = util::stream::read(
      *m_filesystem->open(common::Path("/EncryptedPackage"))->stream());
  std::string decrypted_package = util.decrypt(encrypted_package, key);

  auto memory_file =
      std::make_shared<common::MemoryFile>(std::move(decrypted_package));
  auto decrypted = std::make_shared<OfficeOpenXmlFile>(*this);
  decrypted->m_filesystem = zip::ZipFile(memory_file).archive()->filesystem();
  decrypted->m_file_meta = parse_file_meta(*decrypted->m_filesystem);
  decrypted->m_encryption_state = EncryptionState::decrypted;
  return decrypted;
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
