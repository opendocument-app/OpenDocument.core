#include <access/CfbStorage.h>
#include <access/Path.h>
#include <memory>
#include <odr/Exception.h>
#include <oldms/LegacyMicrosoftFile.h>
#include <unordered_map>

namespace odr::oldms {

namespace {
FileMeta parseMeta(const access::ReadStorage &storage) {
  static const std::unordered_map<access::Path, FileType> TYPES = {
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
    if (storage.isFile(t.first)) {
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
    std::shared_ptr<access::ReadStorage> storage)
    : m_storage{std::move(storage)} {
  m_meta = parseMeta(*m_storage);
}

FileType LegacyMicrosoftFile::fileType() const noexcept { return m_meta.type; }

FileMeta LegacyMicrosoftFile::fileMeta() const noexcept { return m_meta; }

bool LegacyMicrosoftFile::passwordEncrypted() const noexcept {
  return m_meta.passwordEncrypted;
}

EncryptionState LegacyMicrosoftFile::encryptionState() const noexcept {
  return EncryptionState::UNKNOWN;
}

bool LegacyMicrosoftFile::decrypt(const std::string &password) {
  return false; // TODO throw
}

std::shared_ptr<common::Document> LegacyMicrosoftFile::document() const {
  return {}; // TODO throw
}

} // namespace odr::oldms