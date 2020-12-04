#ifndef ODR_OLDMS_LEGACYMICROSOFTFILE_H
#define ODR_OLDMS_LEGACYMICROSOFTFILE_H

#include <odr/File.h>
#include <common/File.h>
#include <access/Storage.h>

namespace odr::oldms {

class LegacyMicrosoftFile final : public common::DocumentFile {
public:
  explicit LegacyMicrosoftFile(std::shared_ptr<access::ReadStorage> storage);

  FileType fileType() const noexcept final;
  FileMeta fileMeta() const noexcept final;

  bool passwordEncrypted() const  noexcept final;
  EncryptionState encryptionState() const noexcept final;
  bool decrypt(const std::string &password) final;

  std::shared_ptr<common::Document> document() const final;

private:
  std::shared_ptr<access::ReadStorage> m_storage;
  FileMeta m_meta;
};

} // namespace odr::oldms

#endif // ODR_OLDMS_LEGACYMICROSOFTFILE_H
