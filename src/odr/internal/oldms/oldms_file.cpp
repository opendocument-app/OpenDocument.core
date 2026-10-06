#include <odr/internal/oldms/oldms_file.hpp>

#include <odr/exceptions.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/presentation/ppt_document.hpp>
#include <odr/internal/oldms/presentation/ppt_parser.hpp>
#include <odr/internal/oldms/spreadsheet/xls_document.hpp>
#include <odr/internal/oldms/spreadsheet/xls_parser.hpp>
#include <odr/internal/oldms/text/doc_document.hpp>
#include <odr/internal/oldms/text/doc_parser.hpp>

#include <array>
#include <memory>
#include <optional>
#include <utility>

namespace odr::internal::oldms {

namespace {
/// Probes the cleartext encryption marker; an unreadable marker leaves it
/// unknown.
std::optional<bool>
parse_password_encrypted(const FileType type,
                         const abstract::ReadableFilesystem &files) {
  switch (type) {
  case FileType::legacy_word_document:
    return text::password_encrypted(files);
  case FileType::legacy_powerpoint_presentation:
    return presentation::password_encrypted(files);
  case FileType::legacy_excel_worksheets:
    return spreadsheet::password_encrypted(files);
  default:
    return {};
  }
}

FileMeta parse_meta(const abstract::ReadableFilesystem &files) {
  // Each format MUST have its stream ([MS-DOC], [MS-PPT], [MS-XLS]).
  static constexpr std::array types{
      std::pair{"/WordDocument", FileType::legacy_word_document},
      std::pair{"/PowerPoint Document",
                FileType::legacy_powerpoint_presentation},
      std::pair{"/Workbook", FileType::legacy_excel_worksheets},
  };

  FileMeta result;

  for (const auto &[path, type] : types) {
    if (files.is_file(AbsPath(path))) {
      result.type = type;
      result.mimetype = mimetype_by_file_type(type);
      result.document_type = document_type_by_file_type(type);
      break;
    }
  }

  if (result.type == FileType::unknown) {
    throw UnknownFileType();
  }

  return result;
}
} // namespace

LegacyMicrosoftFile::LegacyMicrosoftFile(
    std::shared_ptr<abstract::ReadableFilesystem> files)
    : m_files{std::move(files)} {
  m_file_meta = parse_meta(*m_files);

  // An unreadable encryption marker leaves the state unknown.
  const std::optional<bool> encrypted =
      parse_password_encrypted(m_file_meta.type, *m_files);
  m_file_meta.password_encrypted = encrypted.value_or(false);
  m_encryption_state = !encrypted.has_value() ? EncryptionState::unknown
                       : *encrypted           ? EncryptionState::encrypted
                                              : EncryptionState::not_encrypted;
}

std::shared_ptr<abstract::File> LegacyMicrosoftFile::file() const noexcept {
  return {};
}

FileType LegacyMicrosoftFile::file_type() const noexcept {
  return m_file_meta.type;
}

std::string_view LegacyMicrosoftFile::mimetype() const noexcept {
  return m_file_meta.mimetype;
}

FileMeta LegacyMicrosoftFile::file_meta() const { return m_file_meta; }

DocumentType LegacyMicrosoftFile::document_type() const {
  return m_file_meta.document_type;
}

bool LegacyMicrosoftFile::password_encrypted() const noexcept {
  return m_file_meta.password_encrypted;
}

EncryptionState LegacyMicrosoftFile::encryption_state() const noexcept {
  return m_encryption_state;
}

std::shared_ptr<abstract::DecodedFile> LegacyMicrosoftFile::decrypt(
    [[maybe_unused]] const std::string &password) const {
  throw UnsupportedOperation(
      "odrcore does not support decryption of legacy Microsoft files");
}

bool LegacyMicrosoftFile::is_decodable() const noexcept {
  return m_encryption_state != EncryptionState::encrypted;
}

std::shared_ptr<abstract::Document> LegacyMicrosoftFile::document() const {
  // The parser would read the ciphertext as structure, and the caller would see
  // a parse error and not a password prompt.
  if (m_encryption_state == EncryptionState::encrypted) {
    throw FileEncryptedError();
  }

  switch (file_type()) {
  case FileType::legacy_word_document:
    return std::make_shared<text::Document>(m_files);
  case FileType::legacy_powerpoint_presentation:
    return std::make_shared<presentation::Document>(m_files);
  case FileType::legacy_excel_worksheets:
    return std::make_shared<spreadsheet::Document>(m_files);
  default:
    throw UnsupportedFileType(file_type());
  }
}

} // namespace odr::internal::oldms
