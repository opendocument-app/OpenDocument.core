#include <odf/OpenDocumentFile.h>
#include <odf/OpenDocument.h>
#include <odf/Crypto.h>
#include <common/XmlUtil.h>
#include <access/Storage.h>
#include <pugixml.hpp>

namespace odr::odf {

OpenDocumentFile::OpenDocumentFile(
    std::shared_ptr<access::ReadStorage> storage) {
  if (!storage->isFile("META-INF/manifest.xml")) {
    auto manifest = common::XmlUtil::parse(*storage, "META-INF/manifest.xml");

    m_file_meta = parseFileMeta(*m_storage, &manifest);
    m_manifest = parseManifest(manifest);
  } else {
    m_file_meta = parseFileMeta(*m_storage, nullptr);
  }

  if (m_file_meta.passwordEncrypted)
    m_encryptionState = EncryptionState::ENCRYPTED;

  m_storage = std::move(storage);
}

EncryptionState OpenDocumentFile::encryptionState() const noexcept {
  return m_encryptionState;
}

bool OpenDocumentFile::decrypt(const std::string &password) {
  // TODO throw if not encrypted or already decrypted
  return Crypto::decrypt(m_storage, m_manifest, password);
}

FileType OpenDocumentFile::fileType() const noexcept {
  return m_file_meta.type;
}

FileMeta OpenDocumentFile::fileMeta() const noexcept {
  FileMeta result = m_file_meta;
  if (m_encryptionState != EncryptionState::ENCRYPTED)
    result.documentMeta = document()->documentMeta();
  return result;
}

std::shared_ptr<common::Document> OpenDocumentFile::document() const {
  // TODO throw if encrypted
  switch (documentType()) {
  case DocumentType::TEXT:
    return std::unique_ptr<OpenDocument>(
        new OpenDocumentText(m_storage));
  case DocumentType::PRESENTATION:
    return std::unique_ptr<OpenDocument>(
        new OpenDocumentPresentation(m_storage));
  case DocumentType::SPREADSHEET:
    return std::unique_ptr<OpenDocument>(
        new OpenDocumentSpreadsheet(m_storage));
  case DocumentType::GRAPHICS:
    return std::unique_ptr<OpenDocument>(
        new OpenDocumentGraphics(m_storage));
  default:
    // TODO throw
    return nullptr;
  }
}

}
