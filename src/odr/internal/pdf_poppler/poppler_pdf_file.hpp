#pragma once

#include <odr/internal/common/file.hpp>

#include <memory>

class PDFDoc;

namespace odr::internal {

class PopplerPdfFile final : public abstract::PdfFile {
public:
  explicit PopplerPdfFile(std::shared_ptr<DiskFile> file);
  explicit PopplerPdfFile(std::shared_ptr<MemoryFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept override;

  [[nodiscard]] FileMeta file_meta() const noexcept override;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept override;

  [[nodiscard]] bool password_encrypted() const noexcept override;
  [[nodiscard]] EncryptionState encryption_state() const noexcept override;
  [[nodiscard]] std::shared_ptr<DecodedFile>
  decrypt(const std::string &password) const override;

  [[nodiscard]] bool is_decodable() const noexcept override;

  [[nodiscard]] PDFDoc &pdf_doc() const;

private:
  std::shared_ptr<abstract::File> m_file;
  std::shared_ptr<PDFDoc> m_pdf_doc;

  EncryptionState m_encryption_state{EncryptionState::unknown};
  FileMeta m_file_meta;

  void open(const std::optional<std::string> &password);
};

} // namespace odr::internal
