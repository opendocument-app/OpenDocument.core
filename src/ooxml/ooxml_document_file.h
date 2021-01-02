#ifndef ODR_OOXML_DOCUMENT_FILE_H
#define ODR_OOXML_DOCUMENT_FILE_H

#include <common/file.h>
#include <odr/file.h>

namespace odr::common {
class ReadStorage;
}

namespace odr::ooxml {

class OfficeOpenXmlFile final : public common::DocumentFile {
public:
  explicit OfficeOpenXmlFile(std::shared_ptr<common::ReadStorage> storage);

  FileType fileType() const noexcept final;
  FileMeta fileMeta() const noexcept final;
  FileLocation fileLocation() const noexcept final;

  std::size_t size() const final;

  std::unique_ptr<std::istream> data() const final;

  bool passwordEncrypted() const noexcept final;
  EncryptionState encryptionState() const noexcept final;
  bool decrypt(const std::string &password) final;

  std::shared_ptr<common::Document> document() const final;

private:
  std::shared_ptr<common::ReadStorage> m_storage;
  FileMeta m_meta;
  EncryptionState m_encryptionState{EncryptionState::NOT_ENCRYPTED};
};

} // namespace odr::ooxml

#endif // ODR_OOXML_DOCUMENT_FILE_H
