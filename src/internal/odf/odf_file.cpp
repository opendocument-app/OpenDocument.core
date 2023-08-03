#include <internal/odf/odf_file.h>

#include <odr/exceptions.h>
#include <odr/file.h>

#include <internal/abstract/filesystem.h>
#include <internal/odf/odf_crypto.h>
#include <internal/odf/odf_document.h>
#include <internal/util/xml_util.h>

#include <utility>

namespace odr::internal::abstract {
class Document;
class File;
} // namespace odr::internal::abstract

namespace odr::internal::odf {

OpenDocumentFile::OpenDocumentFile(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  if (m_filesystem->exists("META-INF/manifest.xml")) {
    auto manifest = util::xml::parse(*m_filesystem, "META-INF/manifest.xml");

    m_file_meta = parse_file_meta(*m_filesystem, &manifest, false);
    m_manifest = parse_manifest(manifest);
  } else {
    m_file_meta = parse_file_meta(*m_filesystem, nullptr, false);
  }

  if (m_file_meta.password_encrypted) {
    m_encryption_state = EncryptionState::encrypted;
  }
}

std::shared_ptr<abstract::File> OpenDocumentFile::file() const noexcept {
  return {};
}

FileType OpenDocumentFile::file_type() const noexcept {
  return m_file_meta.type;
}

FileMeta OpenDocumentFile::file_meta() const noexcept { return m_file_meta; }

DocumentType OpenDocumentFile::document_type() const {
  return m_file_meta.document_meta->document_type;
}

DocumentMeta OpenDocumentFile::document_meta() const {
  return *m_file_meta.document_meta;
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
  m_file_meta = parse_file_meta(*m_filesystem, nullptr, true);
  m_encryption_state = EncryptionState::decrypted;
  return true;
}

std::shared_ptr<abstract::Document> OpenDocumentFile::document() const {
  // TODO throw if encrypted
  switch (file_type()) {
  case FileType::opendocument_text:
    return std::make_shared<Document>(m_file_meta.type, DocumentType::text,
                                      m_filesystem);
  case FileType::opendocument_presentation:
    return std::make_shared<Document>(m_file_meta.type,
                                      DocumentType::presentation, m_filesystem);
  case FileType::opendocument_spreadsheet:
    return std::make_shared<Document>(m_file_meta.type,
                                      DocumentType::spreadsheet, m_filesystem);
  case FileType::opendocument_graphics:
    return std::make_shared<Document>(m_file_meta.type, DocumentType::drawing,
                                      m_filesystem);
  default:
    throw UnsupportedOperation();
  }
}

} // namespace odr::internal::odf
