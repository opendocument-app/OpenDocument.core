#include <internal/abstract/filesystem.h>
#include <internal/cfb/cfb_archive.h>
#include <internal/common/path.h>
#include <internal/oldms/oldms_translator.h>
#include <memory>
#include <odr/exceptions.h>
#include <odr/file_meta.h>
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

LegacyMicrosoft::LegacyMicrosoft(
    std::shared_ptr<abstract::ReadableFilesystem> filesystem) {
  m_meta = parse_meta(*filesystem);
}

LegacyMicrosoft::LegacyMicrosoft(LegacyMicrosoft &&) noexcept = default;

LegacyMicrosoft::~LegacyMicrosoft() = default;

LegacyMicrosoft &
LegacyMicrosoft::operator=(LegacyMicrosoft &&) noexcept = default;

const FileMeta &LegacyMicrosoft::meta() const noexcept { return m_meta; }

bool LegacyMicrosoft::decrypted() const noexcept { return false; }

bool LegacyMicrosoft::translatable() const noexcept { return false; }

bool LegacyMicrosoft::editable() const noexcept { return false; }

bool LegacyMicrosoft::savable(const bool) const noexcept { return false; }

bool LegacyMicrosoft::decrypt(const std::string &) {
  throw UnsupportedOperation();
}

bool LegacyMicrosoft::translate(const common::Path &, const HtmlConfig &) {
  throw UnsupportedOperation();
}

bool LegacyMicrosoft::edit(const std::string &) {
  throw UnsupportedOperation();
}

bool LegacyMicrosoft::save(const common::Path &) const {
  throw UnsupportedOperation();
}

bool LegacyMicrosoft::save(const common::Path &, const std::string &) const {
  throw UnsupportedOperation();
}

} // namespace odr::internal::oldms
