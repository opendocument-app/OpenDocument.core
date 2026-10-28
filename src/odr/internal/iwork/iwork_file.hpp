#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>

#include <memory>
#include <optional>
#include <string>

namespace odr::internal::abstract {
class Document;
class ReadableFilesystem;
} // namespace odr::internal::abstract

namespace odr::internal::iwork {

/// An iWork package, detected from its archives and component list.
class IworkFile final : public abstract::DocumentFile {
public:
  /// @p source is the file the package was read from, if any. An encrypted
  /// package reports @p encrypted_type before decryption; without it, the type
  /// is guessed from the member names.
  explicit IworkFile(std::shared_ptr<abstract::ReadableFilesystem> filesystem,
                     std::shared_ptr<abstract::File> source = nullptr,
                     std::optional<FileType> encrypted_type = std::nullopt);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const override;

  [[nodiscard]] DocumentType document_type() const override;

  [[nodiscard]] bool password_encrypted() const noexcept override;
  [[nodiscard]] EncryptionState encryption_state() const noexcept override;
  [[nodiscard]] std::shared_ptr<DecodedFile>
  decrypt(const std::string &password) const override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const override;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;
  std::shared_ptr<abstract::File> m_source;
  FileMeta m_file_meta;
  EncryptionState m_encryption_state{EncryptionState::not_encrypted};
};

} // namespace odr::internal::iwork
