#ifndef ODR_OLDMS_LEGACYMICROSOFTFILE_H
#define ODR_OLDMS_LEGACYMICROSOFTFILE_H

#include <odr/File.h>
#include <common/File.h>

namespace odr {
namespace access {
class ReadStorage;
}

namespace oldms {

class LegacyMicrosoftFile final : public common::DocumentFile {
public:
  explicit LegacyMicrosoftFile(std::shared_ptr<access::ReadStorage> storage);

  EncryptionState encryptionState() const noexcept final;
  bool decrypt(const std::string &password) final;

  FileType fileType() const noexcept final;
  FileMeta fileMeta() const noexcept final;

  std::shared_ptr<common::Document> document() const final;

private:
  std::shared_ptr<access::ReadStorage> m_storage;
  FileMeta m_meta;
};

} // namespace oldms
} // namespace odr

#endif // ODR_OLDMS_LEGACYMICROSOFTFILE_H
