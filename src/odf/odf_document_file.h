#ifndef ODR_ODF_DOCUMENT_FILE_H
#define ODR_ODF_DOCUMENT_FILE_H

#include <abstract/file.h>
#include <odf/odf_manifest.h>
#include <odr/file.h>

namespace odr::abstract {
class ReadStorage;
}

namespace odr::odf {

class OpenDocumentFile final : public virtual abstract::DocumentFile {
public:
  explicit OpenDocumentFile(std::shared_ptr<abstract::ReadStorage> storage);

  FileType fileType() const noexcept final;
  FileMeta fileMeta() const noexcept final;
  FileLocation fileLocation() const noexcept final;

  std::size_t size() const final;

  std::unique_ptr<std::istream> data() const final;

  bool passwordEncrypted() const noexcept final;
  EncryptionState encryptionState() const noexcept final;
  bool decrypt(const std::string &password) final;

  std::shared_ptr<abstract::Document> document() const final;

private:
  std::shared_ptr<abstract::ReadStorage> m_storage;
  EncryptionState m_encryptionState{EncryptionState::NOT_ENCRYPTED};
  FileMeta m_file_meta;
  Manifest m_manifest;
};

} // namespace odr::odf

#endif // ODR_ODF_DOCUMENT_FILE_H
