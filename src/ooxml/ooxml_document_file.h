#ifndef ODR_OOXML_DOCUMENT_FILE_H
#define ODR_OOXML_DOCUMENT_FILE_H

#include <abstract/file.h>
#include <odr/file.h>

namespace odr::abstract {
class ReadableFilesystem;
}

namespace odr::ooxml {

class OfficeOpenXmlFile final : public abstract::DocumentFile {
public:
  explicit OfficeOpenXmlFile(
      std::shared_ptr<abstract::ReadableFilesystem> storage);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  FileType file_type() const noexcept final;
  FileMeta file_meta() const noexcept final;

  bool password_encrypted() const noexcept final;
  EncryptionState encryption_state() const noexcept final;
  bool decrypt(const std::string &password) final;

  std::shared_ptr<abstract::Document> document() const final;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;
  FileMeta m_meta;
  EncryptionState m_encryptionState{EncryptionState::NOT_ENCRYPTED};
};

} // namespace odr::ooxml

#endif // ODR_OOXML_DOCUMENT_FILE_H
