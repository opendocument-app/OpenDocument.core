#include <odr/internal/odf/odf_file.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/odf/odf_crypto.hpp>
#include <odr/internal/odf/odf_document.hpp>
#include <odr/internal/util/xml_util.hpp>

namespace odr::internal::abstract {
class Document;
class File;
} // namespace odr::internal::abstract

namespace odr::internal::odf {

OpenDocumentFile::OpenDocumentFile(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem)
    : m_filesystem{std::move(filesystem)} {
  if (m_filesystem->exists(AbsPath("/META-INF/manifest.xml"))) {
    auto manifest =
        util::xml::parse(*m_filesystem, AbsPath("/META-INF/manifest.xml"));

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

DecoderEngine OpenDocumentFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

DocumentType OpenDocumentFile::document_type() const {
  return m_file_meta.document_meta.value().document_type;
}

DocumentMeta OpenDocumentFile::document_meta() const {
  return m_file_meta.document_meta.value();
}

bool OpenDocumentFile::password_encrypted() const noexcept {
  return m_file_meta.password_encrypted;
}

EncryptionState OpenDocumentFile::encryption_state() const noexcept {
  return m_encryption_state;
}

std::shared_ptr<abstract::DecodedFile>
OpenDocumentFile::decrypt(const std::string &password) const {
  if (m_encryption_state != EncryptionState::encrypted) {
    throw NotEncryptedError();
  }

  auto decrypted_filesystem = odf::decrypt(m_filesystem, m_manifest, password);
  auto decrypted_file = std::make_shared<OpenDocumentFile>(*this);
  decrypted_file->m_filesystem = std::move(decrypted_filesystem);
  decrypted_file->m_file_meta =
      parse_file_meta(*decrypted_file->m_filesystem, nullptr, true);
  decrypted_file->m_encryption_state = EncryptionState::decrypted;
  return decrypted_file;
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
