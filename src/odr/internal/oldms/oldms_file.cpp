#include <odr/internal/oldms/oldms_file.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/common/path.hpp>
#include <odr/internal/oldms/presentation/ppt_document.hpp>
#include <odr/internal/oldms/presentation/ppt_parser.hpp>
#include <odr/internal/oldms/spreadsheet/xls_document.hpp>
#include <odr/internal/oldms/spreadsheet/xls_parser.hpp>
#include <odr/internal/oldms/text/doc_document.hpp>
#include <odr/internal/oldms/text/doc_parser.hpp>

#include <memory>
#include <optional>
#include <unordered_map>

namespace odr::internal::oldms {

namespace {
/// Each format keeps the bytes that say so in the clear, so this is readable
/// without the password. Detection only — odrcore cannot decrypt any of them.
/// Nothing where the format's own probe could not read the signal.
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
  struct Variant {
    FileType type{FileType::unknown};
    DocumentType document_type{DocumentType::unknown};
    std::string_view mimetype;
  };

  static const std::unordered_map<AbsPath, Variant> types = {
      // MS-DOC: The "WordDocument" stream MUST be present in the file.
      // https://msdn.microsoft.com/en-us/library/dd926131(v=office.12).aspx
      {AbsPath("/WordDocument"),
       {FileType::legacy_word_document, DocumentType::text,
        "application/msword"}},
      // MS-PPT: The "PowerPoint Document" stream MUST be present in the file.
      // https://msdn.microsoft.com/en-us/library/dd911009(v=office.12).aspx
      {AbsPath("/PowerPoint Document"),
       {FileType::legacy_powerpoint_presentation, DocumentType::presentation,
        "application/vnd.ms-powerpoint"}},
      // MS-XLS: The "Workbook" stream MUST be present in the file.
      // https://docs.microsoft.com/en-us/openspecs/office_file_formats/ms-ppt/1fc22d56-28f9-4818-bd45-67c2bf721ccf
      {AbsPath("/Workbook"),
       {FileType::legacy_excel_worksheets, DocumentType::spreadsheet,
        "application/vnd.ms-excel"}},
  };

  FileMeta result;

  for (const auto &[path, variant] : types) {
    if (files.is_file(path)) {
      result.type = variant.type;
      result.mimetype = variant.mimetype;
      result.document_type = variant.document_type;
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

  // `EncryptionState::unknown` where the probe could not read the signal: a
  // stream that cannot be inspected is not a document that said it is in the
  // clear, and `FileMeta` has only the boolean to carry it.
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

FileMeta LegacyMicrosoftFile::file_meta() const noexcept { return m_file_meta; }

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

bool LegacyMicrosoftFile::is_decodable() const noexcept { return false; }

std::shared_ptr<abstract::Document> LegacyMicrosoftFile::document() const {
  // otherwise the encrypted bytes get read as structure, and the caller sees a
  // parse error where a password prompt belongs
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
