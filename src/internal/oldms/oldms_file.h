#ifndef ODR_INTERNAL_OLDMS_FILE_H
#define ODR_INTERNAL_OLDMS_FILE_H

#include <odr/file.h>

#include <internal/abstract/file.h>
#include <internal/abstract/filesystem.h>

#include <memory>
#include <string>

namespace odr::internal::abstract {
class Document;
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::oldms {

class LegacyMicrosoftFile final : public abstract::DocumentFile {
public:
  explicit LegacyMicrosoftFile(
      std::shared_ptr<abstract::ReadableFilesystem> storage);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileType file_type() const noexcept final;
  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DocumentType document_type() const final;
  [[nodiscard]] DocumentMeta document_meta() const final;

  [[nodiscard]] bool password_encrypted() const noexcept final;
  [[nodiscard]] EncryptionState encryption_state() const noexcept final;
  bool decrypt(const std::string &password) final;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const final;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_storage;
  FileMeta m_file_meta;
};

} // namespace odr::internal::oldms

#endif // ODR_INTERNAL_OLDMS_FILE_H
