#pragma once

#include <odr/internal/abstract/file.hpp>

namespace odr::internal {

class PdfFile final : public abstract::PdfFile {
public:
  explicit PdfFile(std::shared_ptr<abstract::File> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileMeta file_meta() const noexcept override;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;

  [[nodiscard]] bool password_encrypted() const noexcept override;
  [[nodiscard]] EncryptionState encryption_state() const noexcept override;
  [[nodiscard]] std::shared_ptr<DecodedFile>
  decrypt(const std::string &password) const override;

private:
  std::shared_ptr<abstract::File> m_file;
  FileMeta m_file_meta;
};

} // namespace odr::internal
