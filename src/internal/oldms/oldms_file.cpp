#include <internal/cfb/cfb_archive.h>
#include <internal/common/path.h>
#include <internal/oldms/oldms_file.h>
#include <memory>
#include <odr/exceptions.h>
#include <unordered_map>

namespace odr::internal::oldms {

namespace {
FileMeta parse_meta(const abstract::ReadableFilesystem &storage) {
  static const std::unordered_map<common::Path, FileType> TYPES = {
      // MS-DOC: The "WordDocument" stream MUST be present in the file.
      // https://msdn.microsoft.com/en-us/library/dd926131(v=office.12).aspx
      {"WordDocument", FileType::LEGACY_WORD_DOCUMENT},
      // MS-PPT: The "PowerPoint Document" stream MUST be present in the file.
      // https://msdn.microsoft.com/en-us/library/dd911009(v=office.12).aspx
      {"PowerPoint Document", FileType::LEGACY_POWERPOINT_PRESENTATION},
      // MS-XLS: The "Workbook" stream MUST be present in the file.
      // https://docs.microsoft.com/en-us/openspecs/office_file_formats/ms-ppt/1fc22d56-28f9-4818-bd45-67c2bf721ccf
      {"Workbook", FileType::LEGACY_EXCEL_WORKSHEETS},
  };

  FileMeta result;

  for (auto &&t : TYPES) {
    if (storage.is_file(t.first)) {
      result.type = t.second;
      break;
    }
  }

  if (result.type == FileType::UNKNOWN) {
    throw UnknownFileType();
  }

  return result;
}
} // namespace

LegacyMicrosoftFile::LegacyMicrosoftFile(
    std::shared_ptr<abstract::ReadableFilesystem> storage)
    : m_storage{std::move(storage)} {
  m_meta = parse_meta(*m_storage);
}

std::shared_ptr<abstract::File> LegacyMicrosoftFile::file() const noexcept {
  return {};
}

FileType LegacyMicrosoftFile::file_type() const noexcept { return m_meta.type; }

FileMeta LegacyMicrosoftFile::file_meta() const noexcept { return m_meta; }

bool LegacyMicrosoftFile::password_encrypted() const noexcept {
  return m_meta.password_encrypted;
}

EncryptionState LegacyMicrosoftFile::encryption_state() const noexcept {
  return EncryptionState::UNKNOWN;
}

bool LegacyMicrosoftFile::decrypt(const std::string &password) {
  return false; // TODO throw
}

std::shared_ptr<abstract::Document> LegacyMicrosoftFile::document() const {
  return {}; // TODO throw
}

} // namespace odr::internal::oldms
