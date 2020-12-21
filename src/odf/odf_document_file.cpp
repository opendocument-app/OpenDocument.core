#include <access/storage.h>
#include <common/xml_util.h>
#include <odf/odf_crypto.h>
#include <odf/odf_document.h>
#include <odf/odf_document_file.h>
#include <pugixml.hpp>

namespace odr::odf {

OpenDocumentFile::OpenDocumentFile(std::shared_ptr<access::ReadStorage> storage)
    : m_storage{std::move(storage)} {
  if (m_storage->isFile("META-INF/manifest.xml")) {
    auto manifest = common::XmlUtil::parse(*m_storage, "META-INF/manifest.xml");

    m_file_meta = parseFileMeta(*m_storage, &manifest);
    m_manifest = parseManifest(manifest);
  } else {
    m_file_meta = parseFileMeta(*m_storage, nullptr);
  }

  if (m_file_meta.passwordEncrypted)
    m_encryptionState = EncryptionState::ENCRYPTED;
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

std::unique_ptr<std::istream> OpenDocumentFile::data() const {
  return {}; // TODO
}

bool OpenDocumentFile::passwordEncrypted() const noexcept {
  return m_file_meta.passwordEncrypted;
}

EncryptionState OpenDocumentFile::encryptionState() const noexcept {
  return m_encryptionState;
}

bool OpenDocumentFile::decrypt(const std::string &password) {
  // TODO throw if not encrypted or already decrypted
  if (!Crypto::decrypt(m_storage, m_manifest, password))
    return false;
  m_encryptionState = EncryptionState::DECRYPTED;
  return true;
}

std::shared_ptr<common::Document> OpenDocumentFile::document() const {
  // TODO throw if encrypted
  switch (fileType()) {
  case FileType::OPENDOCUMENT_TEXT:
    return std::make_shared<OpenDocumentText>(m_storage);
  case FileType::OPENDOCUMENT_PRESENTATION:
    return std::make_shared<OpenDocumentPresentation>(m_storage);
  case FileType::OPENDOCUMENT_SPREADSHEET:
    return std::make_shared<OpenDocumentSpreadsheet>(m_storage);
  case FileType::OPENDOCUMENT_GRAPHICS:
    return std::make_shared<OpenDocumentDrawing>(m_storage);
  default:
    // TODO throw
    return nullptr;
  }
}

} // namespace odr::odf
