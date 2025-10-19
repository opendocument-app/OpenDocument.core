#include <odr/internal/oldms/oldms_file.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/common/path.hpp>

#include <memory>
#include <unordered_map>

namespace odr::internal::oldms {

namespace {
FileMeta parse_meta(const abstract::ReadableFilesystem &storage) {
  struct Variant {
    FileType type{FileType::unknown};
    std::string_view mimetype;
  };

  static const std::unordered_map<AbsPath, Variant> types = {
      // MS-DOC: The "WordDocument" stream MUST be present in the file.
      // https://msdn.microsoft.com/en-us/library/dd926131(v=office.12).aspx
      {AbsPath("/WordDocument"),
       {FileType::legacy_word_document, "application/msword"}},
      // MS-PPT: The "PowerPoint Document" stream MUST be present in the file.
      // https://msdn.microsoft.com/en-us/library/dd911009(v=office.12).aspx
      {AbsPath("/PowerPoint Document"),
       {FileType::legacy_powerpoint_presentation,
        "application/vnd.ms-powerpoint"}},
      // MS-XLS: The "Workbook" stream MUST be present in the file.
      // https://docs.microsoft.com/en-us/openspecs/office_file_formats/ms-ppt/1fc22d56-28f9-4818-bd45-67c2bf721ccf
      {AbsPath("/Workbook"),
       {FileType::legacy_excel_worksheets, "application/vnd.ms-excel"}},
  };

  FileMeta result;

  for (const auto &[path, variant] : types) {
    if (storage.is_file(path)) {
      result.type = variant.type;
      result.mimetype = variant.mimetype;
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
    std::shared_ptr<abstract::ReadableFilesystem> storage)
    : m_storage{std::move(storage)} {
  m_file_meta = parse_meta(*m_storage);
}

std::shared_ptr<abstract::File> LegacyMicrosoftFile::file() const noexcept {
  return {};
}

FileType LegacyMicrosoftFile::file_type() const noexcept {
  return m_file_meta.type;
}

FileMeta LegacyMicrosoftFile::file_meta() const noexcept { return m_file_meta; }

DecoderEngine LegacyMicrosoftFile::decoder_engine() const noexcept {
  return DecoderEngine::odr;
}

std::string_view LegacyMicrosoftFile::mimetype() const noexcept {
  return m_file_meta.mimetype;
}

DocumentType LegacyMicrosoftFile::document_type() const {
  return m_file_meta.document_meta.value().document_type;
}

DocumentMeta LegacyMicrosoftFile::document_meta() const {
  return m_file_meta.document_meta.value();
}

bool LegacyMicrosoftFile::password_encrypted() const noexcept {
  return m_file_meta.password_encrypted;
}

EncryptionState LegacyMicrosoftFile::encryption_state() const noexcept {
  return EncryptionState::unknown;
}

std::shared_ptr<abstract::DecodedFile>
LegacyMicrosoftFile::decrypt(const std::string &password) const {
  (void)password;
  throw UnsupportedOperation(
      "odrcore does not support decryption of legacy Microsoft files");
}

bool LegacyMicrosoftFile::is_decodable() const noexcept { return false; }

std::shared_ptr<abstract::Document> LegacyMicrosoftFile::document() const {
  throw UnsupportedFileEncoding(
      "odrcore does not support reading legacy Microsoft files");
}

} // namespace odr::internal::oldms
