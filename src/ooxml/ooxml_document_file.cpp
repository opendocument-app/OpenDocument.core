#include <common/path.h>
#include <ooxml/ooxml_crypto.h>
#include <ooxml/ooxml_document.h>
#include <ooxml/ooxml_document_file.h>
#include <ooxml/ooxml_meta.h>
#include <util/stream_util.h>
#include <zip/zip_storage.h>

namespace odr::ooxml {

OfficeOpenXmlFile::OfficeOpenXmlFile(
    std::shared_ptr<abstract::ReadStorage> storage) {
  m_meta = parseFileMeta(*storage);
  m_storage = std::move(storage);
}

FileType OfficeOpenXmlFile::file_type() const noexcept { return m_meta.type; }

FileMeta OfficeOpenXmlFile::file_meta() const noexcept { return m_meta; }

FileLocation OfficeOpenXmlFile::file_location() const noexcept {
  return FileLocation::UNKNOWN; // TODO
}

std::size_t OfficeOpenXmlFile::size() const {
  return 0; // TODO
}

std::unique_ptr<std::istream> OfficeOpenXmlFile::data() const {
  return {}; // TODO
}

bool OfficeOpenXmlFile::password_encrypted() const noexcept {
  return m_meta.passwordEncrypted;
}

EncryptionState OfficeOpenXmlFile::encryption_state() const noexcept {
  return m_encryptionState;
}

bool OfficeOpenXmlFile::decrypt(const std::string &password) {
  // TODO throw if not encrypted
  // TODO throw if decrypted
  const std::string encryptionInfo =
      util::stream::read(*m_storage->read("EncryptionInfo"));
  // TODO cache Crypto::Util
  Crypto::Util util(encryptionInfo);
  const std::string key = util.deriveKey(password);
  if (!util.verify(key))
    return false;
  const std::string encryptedPackage =
      util::stream::read(*m_storage->read("EncryptedPackage"));
  const std::string decryptedPackage = util.decrypt(encryptedPackage, key);
  m_storage = std::make_unique<zip::ZipReader>(decryptedPackage, false);
  m_meta = parseFileMeta(*m_storage);
  m_encryptionState = EncryptionState::DECRYPTED;
  return true;
}

std::shared_ptr<abstract::Document> OfficeOpenXmlFile::document() const {
  // TODO throw if encrypted
  switch (file_type()) {
  case FileType::OFFICE_OPEN_XML_DOCUMENT:
    return std::make_shared<OfficeOpenXmlTextDocument>(m_storage);
  case FileType::OFFICE_OPEN_XML_PRESENTATION:
    return std::make_shared<OfficeOpenXmlPresentation>(m_storage);
  case FileType::OFFICE_OPEN_XML_WORKBOOK:
    return std::make_shared<OfficeOpenXmlSpreadsheet>(m_storage);
  default:
    // TODO throw
    return nullptr;
  }
}

} // namespace odr::ooxml
