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

    m_file_meta = parse_file_meta(*m_files, &manifest);
    m_manifest = parse_manifest(manifest);
  } else {
    m_file_meta = parse_file_meta(*m_files, nullptr);
  }

  if (m_file_meta.password_encrypted) {
    m_encryption_state = experimental::EncryptionState::ENCRYPTED;
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

experimental::EncryptionState
OpenDocumentFile::encryption_state() const noexcept {
  return m_encryption_state;
}

bool OpenDocumentFile::decrypt(const std::string &password) {
  // TODO throw if not encrypted or already decrypted
  if (!odf::decrypt(m_files, m_manifest, password)) {
    return false;
  }
  m_encryption_state = experimental::EncryptionState::DECRYPTED;
  return true;
}

experimental::FileMeta OpenDocumentFile::file_meta() const noexcept {
  experimental::FileMeta result = m_file_meta;
  if (m_encryption_state != experimental::EncryptionState::ENCRYPTED) {
    result.document_meta = document()->document_meta();
  }
  return result;
}

std::shared_ptr<abstract::Document> OpenDocumentFile::document() const {
  // TODO throw if encrypted
  switch (file_type()) {
  case FileType::OPENDOCUMENT_TEXT:
    return std::make_shared<OpenDocumentText>(m_files);
  case FileType::OPENDOCUMENT_PRESENTATION:
    return std::make_shared<OpenDocumentPresentation>(m_files);
  case FileType::OPENDOCUMENT_SPREADSHEET:
    return std::make_shared<OpenDocumentSpreadsheet>(m_files);
  case FileType::OPENDOCUMENT_GRAPHICS:
    return std::make_shared<OpenDocumentDrawing>(m_files);
  default:
    // TODO throw
    return {};
  }
}

} // namespace odr::internal::odf
