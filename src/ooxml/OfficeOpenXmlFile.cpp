#include <access/StreamUtil.h>
#include <access/Path.h>
#include <access/ZipStorage.h>
#include <ooxml/Crypto.h>
#include <ooxml/Meta.h>
#include <ooxml/OfficeOpenXmlFile.h>

namespace odr::ooxml {

OfficeOpenXmlFile::OfficeOpenXmlFile(std::shared_ptr<access::ReadStorage> storage) {
  m_meta = Meta::parseFileMeta(*storage);
  m_storage = std::move(storage);
}

FileType OfficeOpenXmlFile::fileType() const noexcept {
  return m_meta.type;
}

FileMeta OfficeOpenXmlFile::fileMeta() const noexcept {
  return m_meta;
}

EncryptionState OfficeOpenXmlFile::encryptionState() const noexcept {
  return m_encryptionState;
}

bool OfficeOpenXmlFile::decrypt(const std::string &password) {
  // TODO throw if not encrypted
  // TODO throw if decrypted
  const std::string encryptionInfo =
      access::StreamUtil::read(*m_storage->read("EncryptionInfo"));
  // TODO cache Crypto::Util
  Crypto::Util util(encryptionInfo);
  const std::string key = util.deriveKey(password);
  if (!util.verify(key))
    return false;
  const std::string encryptedPackage =
      access::StreamUtil::read(*m_storage->read("EncryptedPackage"));
  const std::string decryptedPackage = util.decrypt(encryptedPackage, key);
  m_storage = std::make_unique<access::ZipReader>(decryptedPackage, false);
  m_meta = Meta::parseFileMeta(*m_storage);
  m_encryptionState = EncryptionState::DECRYPTED;
  return true;
}

std::shared_ptr<common::Document> OfficeOpenXmlFile::document() const {
  return {}; // TODO
}

}
