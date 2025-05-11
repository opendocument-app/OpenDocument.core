#pragma once

#include <odr/internal/common/file.hpp>

#include <memory>

class PDFDoc;

namespace odr::internal {

class PopplerPdfFile final : public abstract::PdfFile {
public:
  explicit PopplerPdfFile(std::shared_ptr<common::DiskFile> file);
  explicit PopplerPdfFile(std::shared_ptr<common::MemoryFile> file);

  [[nodiscard]] std::shared_ptr<abstract::File> file() const noexcept final;

  [[nodiscard]] FileMeta file_meta() const noexcept final;
  [[nodiscard]] DecoderEngine decoder_engine() const noexcept final;

  [[nodiscard]] bool password_encrypted() const noexcept final;
  [[nodiscard]] EncryptionState encryption_state() const noexcept final;
  [[nodiscard]] bool decrypt(const std::string &password) final;

  [[nodiscard]] PDFDoc &pdf_doc() const;

private:
  std::shared_ptr<abstract::File> m_file;
  std::shared_ptr<PDFDoc> m_pdf_doc;

  EncryptionState m_encryption_state{EncryptionState::unknown};
  FileMeta m_file_meta;

  void open(const std::optional<std::string> &password);
};

} // namespace odr::internal
