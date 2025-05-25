#include <odr/internal/pdf/pdf_file.hpp>

namespace odr::internal {

PdfFile::PdfFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {}

std::shared_ptr<abstract::File> PdfFile::file() const noexcept {
  return m_file;
}

FileMeta PdfFile::file_meta() const noexcept { return {}; }

DecoderEngine PdfFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

bool PdfFile::password_encrypted() const noexcept { return false; }

EncryptionState PdfFile::encryption_state() const noexcept {
  return EncryptionState::not_encrypted;
}

std::shared_ptr<abstract::DecodedFile>
PdfFile::decrypt(const std::string &password) const noexcept {
  return nullptr;
}

} // namespace odr::internal
