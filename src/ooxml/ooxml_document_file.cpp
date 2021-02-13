#include <abstract/file.h>
#include <abstract/filesystem.h>
#include <common/path.h>
#include <ooxml/ooxml_crypto.h>
#include <ooxml/ooxml_document.h>
#include <ooxml/ooxml_document_file.h>
#include <ooxml/ooxml_meta.h>
#include <util/stream_util.h>

namespace odr::ooxml {

OfficeOpenXmlFile::OfficeOpenXmlFile(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem) {
  m_meta = parse_file_meta(*filesystem);
  m_filesystem = std::move(filesystem);
}

std::shared_ptr<abstract::File> OfficeOpenXmlFile::file() const noexcept {
  return {};
}

FileType OfficeOpenXmlFile::file_type() const noexcept { return m_meta.type; }

FileMeta OfficeOpenXmlFile::file_meta() const noexcept { return m_meta; }

bool OfficeOpenXmlFile::password_encrypted() const noexcept {
  return m_meta.password_encrypted;
}

EncryptionState OfficeOpenXmlFile::encryption_state() const noexcept {
  return m_encryptionState;
}

bool OfficeOpenXmlFile::decrypt(const std::string &password) {
  // TODO throw if not encrypted
  // TODO throw if decrypted
  const std::string encryptionInfo =
      util::stream::read(*m_filesystem->open("EncryptionInfo")->read());
  // TODO cache Crypto::Util
  Crypto::Util util(encryptionInfo);
  const std::string key = util.deriveKey(password);
  if (!util.verify(key)) {
    return false;
  }
  const std::string encryptedPackage =
      util::stream::read(*m_filesystem->open("EncryptedPackage")->read());
  const std::string decryptedPackage = util.decrypt(encryptedPackage, key);
  // TODO
  // m_filesystem = std::make_unique<zip::ZipReader>(decryptedPackage, false);
  m_meta = parse_file_meta(*m_filesystem);
  m_encryptionState = EncryptionState::DECRYPTED;
  return true;
}

std::shared_ptr<abstract::Document> OfficeOpenXmlFile::document() const {
  // TODO throw if encrypted
  switch (file_type()) {
  case FileType::OFFICE_OPEN_XML_DOCUMENT:
    return std::make_shared<OfficeOpenXmlTextDocument>(m_filesystem);
  case FileType::OFFICE_OPEN_XML_PRESENTATION:
    return std::make_shared<OfficeOpenXmlPresentation>(m_filesystem);
  case FileType::OFFICE_OPEN_XML_WORKBOOK:
    return std::make_shared<OfficeOpenXmlSpreadsheet>(m_filesystem);
  default:
    // TODO throw
    return nullptr;
  }
}

} // namespace odr::ooxml
