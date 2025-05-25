#pragma once

#include <odr/internal/abstract/file.hpp>

namespace odr::internal {

class PdfFile final : public abstract::PdfFile {
public:
  explicit PdfFile(std::shared_ptr<abstract::File> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept final;

  [[nodiscard]] bool password_encrypted() const noexcept final;
  [[nodiscard]] EncryptionState encryption_state() const noexcept final;
  [[nodiscard]] std::shared_ptr<abstract::DecodedFile>
  decrypt(const std::string &password) const noexcept final;

private:
  std::shared_ptr<abstract::File> m_file;
};

} // namespace odr::internal
