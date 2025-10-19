#include <odr/internal/pdf/pdf_file.hpp>

namespace odr::internal {

PdfFile::PdfFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  m_file_meta.type = FileType::portable_document_format;
}

std::shared_ptr<abstract::File> PdfFile::file() const noexcept {
  return m_file;
}

FileMeta PdfFile::file_meta() const noexcept { return m_file_meta; }

DecoderEngine PdfFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

std::string_view PdfFile::mimetype() const noexcept {
  return "application/pdf";
}

bool PdfFile::password_encrypted() const noexcept { return false; }

EncryptionState PdfFile::encryption_state() const noexcept {
  return EncryptionState::not_encrypted;
}

std::shared_ptr<abstract::DecodedFile>
PdfFile::decrypt(const std::string &password) const {
  (void)password;
  return nullptr;
}

bool PdfFile::is_decodable() const noexcept { return false; }

} // namespace odr::internal
