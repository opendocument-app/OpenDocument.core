#include <odr/internal/pdf/pdf_file.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_encryption.hpp>

#include <memory>
#include <string>

namespace odr::internal::pdf {

PdfFile::PdfFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  if (m_file == nullptr) {
    throw std::invalid_argument("pdf: file is null");
  }

  const DocumentParser parser(m_file->stream());

  m_file_meta.type = FileType::portable_document_format;

  // Most "protected" PDFs are owner-locked only, so try the empty user
  // password first; if it opens the file, no password is required.
  m_authenticator = parser.authenticator();
  if (m_authenticator.has_value()) {
    m_decryptor = m_authenticator->authenticate("");
  }

  if (parser.is_encrypted()) {
    // Owner-locked only: opens with the empty user password. Keep the
    // decryptor so rendering needs neither the password nor a decrypt() call.
    m_file_meta.password_encrypted = !m_decryptor.has_value();
    m_encryption_state = m_decryptor.has_value()
                             ? EncryptionState::not_encrypted
                             : EncryptionState::encrypted;
  }
}

std::shared_ptr<abstract::File> PdfFile::file() const noexcept {
  return m_file;
}

DecoderEngine PdfFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

FileMeta PdfFile::file_meta() const noexcept { return m_file_meta; }

bool PdfFile::password_encrypted() const noexcept {
  return m_encryption_state == EncryptionState::encrypted;
}

EncryptionState PdfFile::encryption_state() const noexcept {
  return m_encryption_state;
}

std::shared_ptr<abstract::DecodedFile>
PdfFile::decrypt(const std::string &password) const {
  if (m_encryption_state != EncryptionState::encrypted) {
    throw NotEncryptedError();
  }
  if (!m_authenticator.has_value()) {
    throw std::logic_error("pdf: no authenticator for encrypted file");
  }
  std::optional<Decryptor> decryptor = m_authenticator->authenticate(password);
  if (!decryptor.has_value()) {
    throw WrongPasswordError();
  }

  auto decrypted = std::make_shared<PdfFile>(*this);
  decrypted->m_decryptor = std::move(decryptor);
  decrypted->m_encryption_state = EncryptionState::decrypted;
  decrypted->m_file_meta.password_encrypted = false;
  return decrypted;
}

bool PdfFile::is_decodable() const noexcept {
  return m_encryption_state != EncryptionState::encrypted;
}

DocumentParser PdfFile::create_parser(const Logger &logger) const {
  return DocumentParser(m_file->stream(), m_decryptor, logger);
}

} // namespace odr::internal::pdf
