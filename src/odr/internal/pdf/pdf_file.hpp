#pragma once

#include <odr/internal/abstract/file.hpp>

#include <optional>
#include <string>

namespace odr::internal {

class PdfFile final : public abstract::PdfFile {
public:
  explicit PdfFile(std::shared_ptr<abstract::File> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;
  [[nodiscard]] FileMeta file_meta() const noexcept override;

  [[nodiscard]] bool password_encrypted() const noexcept override;
  [[nodiscard]] EncryptionState encryption_state() const noexcept override;
  [[nodiscard]] std::shared_ptr<DecodedFile>
  decrypt(const std::string &password) const override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  /// The derived file key that unlocks this file (`nullopt` for files that open
  /// without decryption), for the HTML service to feed the document parser.
  [[nodiscard]] std::optional<std::string>
  decryption_key() const noexcept override;

private:
  std::shared_ptr<abstract::File> m_file;
  FileMeta m_file_meta;
  EncryptionState m_encryption_state{EncryptionState::not_encrypted};
  /// The file encryption key derived when the file was unlocked; the user's
  /// password is never stored. See [[pdf-encryption]] `Decryptor::file_key`.
  std::optional<std::string> m_decryption_key;
};

} // namespace odr::internal
