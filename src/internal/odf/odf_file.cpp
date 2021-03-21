#include <internal/abstract/filesystem.h>
#include <internal/odf/odf_crypto.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_file.h>
#include <internal/util/xml_util.h>
#include <pugixml.hpp>

namespace odr::internal::odf {

OpenDocumentFile::OpenDocumentFile(
    std::shared_ptr<abstract::ReadableFilesystem> files)
    : m_files{std::move(files)} {
  if (m_files->exists("META-INF/manifest.xml")) {
    auto manifest = util::xml::parse(*m_files, "META-INF/manifest.xml");

    m_file_meta = parse_file_meta(*m_files, &manifest, true);
    m_manifest = parse_manifest(manifest);
  } else {
    m_file_meta = parse_file_meta(*m_files, nullptr, true);
  }

  if (m_file_meta.password_encrypted) {
    m_encryption_state = EncryptionState::ENCRYPTED;
  }
}

std::shared_ptr<abstract::File> OpenDocumentFile::file() const noexcept {
  return {};
}

FileType OpenDocumentFile::file_type() const noexcept {
  return m_file_meta.type;
}

bool OpenDocumentFile::password_encrypted() const noexcept {
  return m_file_meta.password_encrypted;
}

EncryptionState OpenDocumentFile::encryption_state() const noexcept {
  return m_encryption_state;
}

bool OpenDocumentFile::decrypt(const std::string &password) {
  // TODO throw if not encrypted or already decrypted
  if (!odf::decrypt(m_files, m_manifest, password)) {
    return false;
  }
  m_encryption_state = EncryptionState::DECRYPTED;
  return true;
}

FileMeta OpenDocumentFile::file_meta() const noexcept { return m_file_meta; }

std::shared_ptr<abstract::Document> OpenDocumentFile::document() const {
  // TODO throw if encrypted
  return std::make_shared<OpenDocument>(m_files);
}

} // namespace odr::internal::odf
