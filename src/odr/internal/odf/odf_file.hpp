#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/odf/odf_manifest.hpp>

#include <memory>
#include <string>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::odf {

class OpenDocumentFile final : public virtual abstract::DocumentFile {
public:
  explicit OpenDocumentFile(
      std::shared_ptr<abstract::ReadableFilesystem> filesystem);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept final;

  [[nodiscard]] DocumentType document_type() const final;
  [[nodiscard]] DocumentMeta document_meta() const final;

  [[nodiscard]] bool password_encrypted() const noexcept final;
  [[nodiscard]] EncryptionState encryption_state() const noexcept final;
  [[nodiscard]] std::shared_ptr<abstract::DecodedFile>
  decrypt(const std::string &password) const final;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const final;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;
  EncryptionState m_encryption_state{EncryptionState::not_encrypted};
  FileMeta m_file_meta;
  Manifest m_manifest;
};

} // namespace odr::internal::odf
