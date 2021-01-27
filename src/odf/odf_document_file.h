#ifndef ODR_ODF_DOCUMENT_FILE_H
#define ODR_ODF_DOCUMENT_FILE_H

#include <abstract/file.h>
#include <odf/odf_manifest.h>
#include <odr/file.h>

namespace odr::abstract {
class ReadableFilesystem;
}

namespace odr::odf {

class OpenDocumentFile final : public virtual abstract::DocumentFile {
public:
  explicit OpenDocumentFile(
      std::shared_ptr<abstract::ReadableFilesystem> files);

  FileType file_type() const noexcept final;
  FileMeta file_meta() const noexcept final;

  bool password_encrypted() const noexcept final;
  EncryptionState encryption_state() const noexcept final;
  bool decrypt(const std::string &password) final;

  std::shared_ptr<abstract::Document> document() const final;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_files;
  EncryptionState m_encryption_state{EncryptionState::NOT_ENCRYPTED};
  FileMeta m_file_meta;
  Manifest m_manifest;
};

} // namespace odr::odf

#endif // ODR_ODF_DOCUMENT_FILE_H
