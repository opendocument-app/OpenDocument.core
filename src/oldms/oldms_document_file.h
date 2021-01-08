#ifndef ODR_OLDMS_DOCUMENT_FILE_H
#define ODR_OLDMS_DOCUMENT_FILE_H

#include <abstract/file.h>
#include <abstract/storage.h>
#include <odr/file.h>

namespace odr::oldms {

class LegacyMicrosoftFile final : public abstract::DocumentFile {
public:
  explicit LegacyMicrosoftFile(std::shared_ptr<abstract::ReadStorage> storage);

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
};

} // namespace odr::oldms

#endif // ODR_OLDMS_DOCUMENT_FILE_H
