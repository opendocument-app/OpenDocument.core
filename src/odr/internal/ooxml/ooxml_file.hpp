#pragma once

#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>

#include <memory>
#include <string>

namespace odr::internal::abstract {
class Document;
} // namespace odr::internal::abstract

namespace odr::internal::abstract {
class ReadableFilesystem;
}

namespace odr::internal::ooxml {

class OfficeOpenXmlFile final : public abstract::DocumentFile {
public:
  explicit OfficeOpenXmlFile(
      std::shared_ptr<abstract::ReadableFilesystem> storage);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;
  [[nodiscard]] FileType file_type() const noexcept override;
  [[nodiscard]] std::string_view mimetype() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] DocumentType document_type() const override;
  [[nodiscard]] DocumentMeta document_meta() const override;

  [[nodiscard]] bool password_encrypted() const noexcept override;
  [[nodiscard]] EncryptionState encryption_state() const noexcept override;
  [[nodiscard]] std::shared_ptr<DecodedFile>
  decrypt(const std::string &password) const override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] std::shared_ptr<abstract::Document> document() const override;

private:
  std::shared_ptr<abstract::ReadableFilesystem> m_filesystem;
  FileMeta m_file_meta;
  EncryptionState m_encryption_state{EncryptionState::not_encrypted};
};

} // namespace odr::internal::ooxml
