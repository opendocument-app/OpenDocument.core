#pragma once

#include <odr/logger.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/pdf/pdf_encryption.hpp>

#include <memory>
#include <string>

namespace odr::internal::pdf {

class DocumentParser;
class Decryptor;

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

  [[nodiscard]] DocumentParser
  create_parser(const Logger &logger = Logger::null()) const;

private:
  std::shared_ptr<abstract::File> m_file;
  std::optional<Authenticator> m_authenticator;
  std::optional<Decryptor> m_decryptor;

  FileMeta m_file_meta;
  EncryptionState m_encryption_state{EncryptionState::not_encrypted};
};

} // namespace odr::internal::pdf
