#include <internal/abstract/filesystem.h>
#include <internal/odf/odf_crypto.h>
#include <internal/odf/odf_document.h>
#include <internal/odf/odf_file.h>
#include <internal/util/xml_util.h>
#include <odr/exceptions.h>
#include <pugixml.hpp>

namespace odr::internal::odf {

OpenDocumentFile::OpenDocumentFile(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  if (m_filesystem->exists("META-INF/manifest.xml")) {
    auto manifest = util::xml::parse(*m_filesystem, "META-INF/manifest.xml");

    m_file_meta = parse_file_meta(*m_filesystem, &manifest, true);
    m_manifest = parse_manifest(manifest);
  } else {
    m_file_meta = parse_file_meta(*m_filesystem, nullptr, true);
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
  if (!odf::decrypt(m_filesystem, m_manifest, password)) {
    return false;
  }
  m_encryption_state = EncryptionState::DECRYPTED;
  return true;
}

FileMeta OpenDocumentFile::file_meta() const noexcept { return m_file_meta; }

std::shared_ptr<abstract::Document> OpenDocumentFile::document() const {
  // TODO throw if encrypted
  switch (file_type()) {
  case FileType::OPENDOCUMENT_TEXT:
    return std::make_shared<OpenDocument>(DocumentType::TEXT, m_filesystem);
  case FileType::OPENDOCUMENT_PRESENTATION:
    return std::make_shared<OpenDocument>(DocumentType::PRESENTATION,
                                          m_filesystem);
  case FileType::OPENDOCUMENT_SPREADSHEET:
    return std::make_shared<OpenDocument>(DocumentType::SPREADSHEET,
                                          m_filesystem);
  case FileType::OPENDOCUMENT_GRAPHICS:
    return std::make_shared<OpenDocument>(DocumentType::DRAWING, m_filesystem);
  default:
    throw UnsupportedOperation();
  }
}

} // namespace odr::internal::odf
