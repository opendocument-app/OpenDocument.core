#ifndef ODR_OOXML_OFFICEOPENXMLFILE_H
#define ODR_OOXML_OFFICEOPENXMLFILE_H

#include <odr/File.h>
#include <common/File.h>

namespace odr {
namespace access {
class ReadStorage;
}

namespace ooxml {

class OfficeOpenXmlFile final : public common::DocumentFile {
public:
  explicit OfficeOpenXmlFile(std::shared_ptr<access::ReadStorage> storage);

  EncryptionState encryptionState() const noexcept final;
  bool decrypt(const std::string &password) final;

  FileType fileType() const noexcept final;
  FileMeta fileMeta() const noexcept final;

  std::shared_ptr<common::Document> document() const final;

private:
  std::shared_ptr<access::ReadStorage> m_storage;
  FileMeta m_meta;
};

} // namespace ooxml
} // namespace odr

#endif // ODR_OOXML_OFFICEOPENXMLFILE_H
