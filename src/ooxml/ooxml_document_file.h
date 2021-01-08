#ifndef ODR_OOXML_DOCUMENT_FILE_H
#define ODR_OOXML_DOCUMENT_FILE_H

#include <abstract/file.h>
#include <odr/file.h>

namespace odr::abstract {
class ReadStorage;
}

namespace odr::ooxml {

class OfficeOpenXmlFile final : public abstract::DocumentFile {
public:
  explicit OfficeOpenXmlFile(std::shared_ptr<abstract::ReadStorage> storage);

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
  FileMeta m_meta;
  EncryptionState m_encryptionState{EncryptionState::NOT_ENCRYPTED};
};

} // namespace odr::ooxml

#endif // ODR_OOXML_DOCUMENT_FILE_H
