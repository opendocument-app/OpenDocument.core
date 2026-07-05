#include <odr/internal/pdf/pdf_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_encoding.hpp>
#include <odr/internal/pdf/pdf_encryption.hpp>
#include <odr/internal/pdf/pdf_object.hpp>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>

namespace odr::internal::pdf {

namespace {

/// One `/Info` string entry, resolved and decoded to UTF-8 (`nullopt` when
/// absent, not a string, or empty).
std::optional<std::string> info_string(DocumentParser &parser,
                                       const Dictionary &info,
                                       const std::string &key) {
  if (!info.has_value(key)) {
    return std::nullopt;
  }
  const Object value = parser.resolve_object_copy(info.get(key));
  if (!value.is_string()) {
    return std::nullopt;
  }
  std::string decoded = decode_text_string(value.as_string());
  if (decoded.empty()) {
    return std::nullopt;
  }
  return decoded;
}

/// Document metadata from the trailer: the page count (the root `/Pages
/// /Count`, a single lookup — no tree walk) and the `/Info` document
/// information dictionary (ISO 32000-1 14.3.3). Reads may throw on a malformed
/// file; the caller treats that as "no metadata".
DocumentMeta parse_document_meta(DocumentParser &parser) {
  DocumentMeta meta;
  meta.document_type = DocumentType::text;

  const Dictionary &trailer = parser.trailer();

  if (trailer.has_value("Root")) {
    const Object root = parser.resolve_object_copy(trailer.get("Root"));
    if (root.is_dictionary() && root.as_dictionary().has_value("Pages")) {
      const Object pages =
          parser.resolve_object_copy(root.as_dictionary().get("Pages"));
      if (pages.is_dictionary() && pages.as_dictionary().has_value("Count")) {
        const Object count =
            parser.resolve_object_copy(pages.as_dictionary().get("Count"));
        if (count.is_integer() && count.as_integer() >= 0) {
          meta.entry_count = static_cast<std::uint32_t>(count.as_integer());
        }
      }
    }
  }

  if (trailer.has_value("Info")) {
    const Object info = parser.resolve_object_copy(trailer.get("Info"));
    if (info.is_dictionary()) {
      const Dictionary &d = info.as_dictionary();
      meta.title = info_string(parser, d, "Title");
      meta.author = info_string(parser, d, "Author");
      meta.subject = info_string(parser, d, "Subject");
      meta.keywords = info_string(parser, d, "Keywords");
      meta.creator = info_string(parser, d, "Creator");
      meta.producer = info_string(parser, d, "Producer");
      meta.creation_date = info_string(parser, d, "CreationDate");
      meta.modification_date = info_string(parser, d, "ModDate");
    }
  }

  return meta;
}

} // namespace

PdfFile::PdfFile(std::shared_ptr<abstract::File> file)
    : m_file{std::move(file)} {
  if (m_file == nullptr) {
    throw std::invalid_argument("pdf: file is null");
  }

  DocumentParser parser(m_file->stream());

  m_file_meta.type = FileType::portable_document_format;

  m_authenticator = parser.authenticator();
  if (parser.is_encrypted()) {
    // Most "protected" PDFs are owner-locked only, so try the empty user
    // password first; if it opens the file, no password is required. Install
    // the resulting decryptor on `parser` so the metadata read below decrypts,
    // and keep a copy so rendering needs neither the password nor decrypt().
    const bool unlocked = parser.authenticate("");
    if (unlocked) {
      m_decryptor = parser.decryptor();
    }
    m_file_meta.password_encrypted = !unlocked;
    m_encryption_state =
        unlocked ? EncryptionState::not_encrypted : EncryptionState::encrypted;
  }

  // Best-effort document metadata (page count + `/Info`), only when the file is
  // readable. Never fatal: a malformed structure leaves the metadata minimal.
  if (m_encryption_state != EncryptionState::encrypted) {
    try {
      m_file_meta.document_meta = parse_document_meta(parser);
    } catch (...) { // NOLINT(bugprone-empty-catch): metadata is best-effort, a
                    // malformed file simply carries no `document_meta`.
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
