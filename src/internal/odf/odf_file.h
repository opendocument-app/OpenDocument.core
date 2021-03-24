#ifndef ODR_INTERNAL_ODF_FILE_H
#define ODR_INTERNAL_ODF_FILE_H

#include <internal/abstract/file.h>
#include <internal/odf/odf_manifest.h>
#include <odr/file.h>

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

  [[nodiscard]] bool password_encrypted() const noexcept final;
  [[nodiscard]] EncryptionState encryption_state() const noexcept final;
  bool decrypt(const std::string &password) final;

  [[nodiscard]] FileMeta file_meta() const noexcept final;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const final;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;
  EncryptionState m_encryption_state{EncryptionState::NOT_ENCRYPTED};
  FileMeta m_file_meta;
  Manifest m_manifest;
};

} // namespace odr::internal::odf

#endif // ODR_INTERNAL_ODF_FILE_H
