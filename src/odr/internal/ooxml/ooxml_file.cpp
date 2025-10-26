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

namespace odr::internal::ooxml {

OfficeOpenXmlFile::OfficeOpenXmlFile(
    std::shared_ptr<abstract::ReadableFilesystem> files)
    : m_files(std::move(files)) {
  m_file_meta = parse_file_meta(*m_files);

  if (m_file_meta.password_encrypted) {
    m_encryption_state = EncryptionState::encrypted;
  }
}

std::shared_ptr<abstract::File> OfficeOpenXmlFile::file() const noexcept {
  return {};
}

DecoderEngine OfficeOpenXmlFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

FileType OfficeOpenXmlFile::file_type() const noexcept {
  return m_file_meta.type;
}

std::string_view OfficeOpenXmlFile::mimetype() const noexcept {
  return m_file_meta.mimetype;
}

FileMeta OfficeOpenXmlFile::file_meta() const noexcept { return m_file_meta; }

DocumentType OfficeOpenXmlFile::document_type() const {
  return m_file_meta.document_meta.value().document_type;
}

DocumentMeta OfficeOpenXmlFile::document_meta() const {
  return m_file_meta.document_meta.value();
}

bool OfficeOpenXmlFile::password_encrypted() const noexcept {
  return m_file_meta.password_encrypted;
}

EncryptionState OfficeOpenXmlFile::encryption_state() const noexcept {
  return m_encryption_state;
}

std::shared_ptr<abstract::DecodedFile>
OfficeOpenXmlFile::decrypt(const std::string &password) const {
  if (m_encryption_state != EncryptionState::encrypted) {
    throw NotEncryptedError();
  }

  const std::string encryption_info =
      util::stream::read(*m_files->open(AbsPath("/EncryptionInfo"))->stream());
  // TODO cache Crypto::Util
  const crypto::Util util(encryption_info);
  const std::string key = util.derive_key(password);
  if (!util.verify(key)) {
    throw WrongPasswordError();
  }

  const std::string encrypted_package = util::stream::read(
      *m_files->open(AbsPath("/EncryptedPackage"))->stream());
  std::string decrypted_package = util.decrypt(encrypted_package, key);

  const auto memory_file =
      std::make_shared<MemoryFile>(std::move(decrypted_package));
  auto decrypted = std::make_shared<OfficeOpenXmlFile>(*this);
  decrypted->m_files = zip::ZipFile(memory_file).archive()->as_filesystem();
  decrypted->m_file_meta = parse_file_meta(*decrypted->m_files);
  decrypted->m_encryption_state = EncryptionState::decrypted;
  return decrypted;
}

bool OfficeOpenXmlFile::is_decodable() const noexcept {
  return m_encryption_state != EncryptionState::encrypted;
}

std::shared_ptr<abstract::Document> OfficeOpenXmlFile::document() const {
  if (m_encryption_state == EncryptionState::encrypted) {
    throw FileEncryptedError();
  }

  switch (file_type()) {
  case FileType::office_open_xml_document:
    return std::make_shared<text::Document>(m_files);
  case FileType::office_open_xml_presentation:
    return std::make_shared<presentation::Document>(m_files);
  case FileType::office_open_xml_workbook:
    return std::make_shared<spreadsheet::Document>(m_files);
  default:
    throw UnsupportedFileType(file_type());
  }
}

} // namespace odr::internal::ooxml
