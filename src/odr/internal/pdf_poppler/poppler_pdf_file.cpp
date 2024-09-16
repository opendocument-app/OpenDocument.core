#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>

#include <poppler/PDFDocFactory.h>
#include <poppler/goo/GooString.h>

namespace odr::internal {

PopplerPdfFile::PopplerPdfFile(std::shared_ptr<common::DiskFile> file)
    : m_file{std::move(file)} {
  open(std::nullopt);
}

void PopplerPdfFile::open(const std::optional<std::string> &password) {
  GooString file_path_goo(m_file->disk_path()->string());
  std::optional<GooString> password_goo;
  if (password.has_value()) {
    password_goo = GooString(password.value().c_str());
  }

  m_pdf_doc = std::shared_ptr<PDFDoc>(
      PDFDocFactory().createPDFDoc(file_path_goo, password_goo, password_goo));

  if (!m_pdf_doc->isOk()) {
    if (m_pdf_doc->getErrorCode() == errEncrypted) {
      m_encryption_state = EncryptionState::encrypted;
    } else {
      throw std::runtime_error("Failed to open PDF file");
    }
  } else {
    m_encryption_state = m_pdf_doc->isEncrypted()
                             ? EncryptionState::decrypted
                             : EncryptionState::not_encrypted;
  }
}

std::shared_ptr<abstract::File> PopplerPdfFile::file() const noexcept {
  return m_file;
}

FileMeta PopplerPdfFile::file_meta() const noexcept { return {}; }

DecoderEngine PopplerPdfFile::decoder_engine() const noexcept {
  return DecoderEngine::poppler;
}

bool PopplerPdfFile::password_encrypted() const noexcept {
  return m_encryption_state == EncryptionState::encrypted ||
         m_encryption_state == EncryptionState::decrypted;
}

EncryptionState PopplerPdfFile::encryption_state() const noexcept {
  return m_encryption_state;
}

bool PopplerPdfFile::decrypt(const std::string &password) {
  if (encryption_state() != EncryptionState::encrypted) {
    return false;
  }

  open(password);

  return encryption_state() == EncryptionState::decrypted;
}

PDFDoc &PopplerPdfFile::pdf_doc() const { return *m_pdf_doc; }

} // namespace odr::internal
