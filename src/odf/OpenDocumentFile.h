#ifndef ODR_ODF_OPENDOCUMENTFILE_H
#define ODR_ODF_OPENDOCUMENTFILE_H

#include <odr/File.h>
#include <common/File.h>
#include <odf/Manifest.h>

namespace odr {
namespace access {
class ReadStorage;
}

namespace odf {

class OpenDocumentFile final : public virtual common::DocumentFile {
public:
  explicit OpenDocumentFile(std::shared_ptr<access::ReadStorage> storage);

  EncryptionState encryptionState() const noexcept final;
  bool decrypt(const std::string &password) final;

  FileType fileType() const noexcept final;
  FileMeta fileMeta() const noexcept final;

  std::shared_ptr<common::Document> document() const final;

private:
  std::shared_ptr<access::ReadStorage> m_storage;
  EncryptionState m_encryptionState;
  FileMeta m_file_meta;
  Manifest m_manifest;
};

}
}

#endif // ODR_ODF_OPENDOCUMENTFILE_H
