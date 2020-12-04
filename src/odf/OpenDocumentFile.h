#ifndef ODR_ODF_OPENDOCUMENTFILE_H
#define ODR_ODF_OPENDOCUMENTFILE_H

#include <common/File.h>
#include <odf/Manifest.h>
#include <odr/File.h>

namespace odr {
namespace access {
class ReadStorage;
}

namespace odf {

class OpenDocumentFile final : public virtual common::DocumentFile {
public:
  explicit OpenDocumentFile(std::shared_ptr<access::ReadStorage> storage);

  FileType fileType() const noexcept final;
  FileMeta fileMeta() const noexcept final;

  bool passwordEncrypted() const noexcept final;
  EncryptionState encryptionState() const noexcept final;
  bool decrypt(const std::string &password) final;

  std::shared_ptr<common::Document> document() const final;

private:
  std::shared_ptr<access::ReadStorage> m_storage;
  EncryptionState m_encryptionState;
  FileMeta m_file_meta;
  Manifest m_manifest;
};

} // namespace odf
} // namespace odr

#endif // ODR_ODF_OPENDOCUMENTFILE_H
