#include <odr/internal/pdf/pdf_file.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_encryption.hpp>

#include <istream>
#include <memory>
#include <string>

namespace odr::internal {

namespace {

/// Walk the trailer chain just far enough to learn whether the file is
/// encrypted and whether `password` unlocks it. Defensive: any parse failure
/// is reported as "not encrypted" so file detection never breaks on a
/// malformed PDF — the real error then surfaces when HTML is requested.
struct ProbeResult {
  bool encrypted{false};
  bool authenticated{true};
  /// The authenticated decryptor when `authenticated` (carried in place of the
  /// password); `nullptr` otherwise.
  std::shared_ptr<const pdf::Decryptor> decryptor;
};

ProbeResult probe_encryption(const abstract::File &file,
                             const std::string &password) {
  try {
    const std::unique_ptr<std::istream> in = file.stream();
    pdf::DocumentParser parser(*in);
    parser.probe_encryption(password);
    return {parser.encrypted(), parser.authenticated(), parser.decryptor()};
  } catch (...) {
    return {false, true, nullptr};
  }
}

} // namespace

PdfFile::PdfFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  m_file_meta.type = FileType::portable_document_format;

  // Most "protected" PDFs are owner-locked only, so try the empty user
  // password first; if it opens the file, no password is required.
  const ProbeResult probe = probe_encryption(*m_file, "");
  if (probe.encrypted) {
    m_file_meta.password_encrypted = !probe.authenticated;
    if (probe.authenticated) {
      // Owner-locked only: opens with the empty user password. Keep the
      // decryptor so rendering needs neither the password nor a decrypt() call.
      m_encryption_state = EncryptionState::not_encrypted;
      m_decryptor = probe.decryptor;
    } else {
      m_encryption_state = EncryptionState::encrypted;
    }
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
  const ProbeResult probe = probe_encryption(*m_file, password);
  if (!probe.authenticated) {
    throw WrongPasswordError();
  }

  auto decrypted = std::make_shared<PdfFile>(*this);
  // Carry the authenticated decryptor and drop the password.
  decrypted->m_decryptor = probe.decryptor;
  decrypted->m_encryption_state = EncryptionState::decrypted;
  decrypted->m_file_meta.password_encrypted = false;
  return decrypted;
}

bool PdfFile::is_decodable() const noexcept {
  return m_encryption_state != EncryptionState::encrypted;
}

std::shared_ptr<const pdf::Decryptor> PdfFile::decryptor() const noexcept {
  return m_decryptor;
}

} // namespace odr::internal
