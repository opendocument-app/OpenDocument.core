#include <access/CfbStorage.h>
#include <access/Path.h>
#include <memory>
#include <odr/Exception.h>
#include <oldms/LegacyMicrosoft.h>
#include <unordered_map>

namespace odr {
namespace oldms {

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

LegacyMicrosoft::LegacyMicrosoft(const char *path)
    : LegacyMicrosoft(access::Path(path)) {}

LegacyMicrosoft::LegacyMicrosoft(const std::string &path)
    : LegacyMicrosoft(access::Path(path)) {}

LegacyMicrosoft::LegacyMicrosoft(const access::Path &path)
    : LegacyMicrosoft(
          std::unique_ptr<access::ReadStorage>(new access::CfbReader(path))) {}

LegacyMicrosoft::LegacyMicrosoft(
    std::unique_ptr<access::ReadStorage> &&storage) {
  meta_ = parseMeta(*storage);
}

LegacyMicrosoft::LegacyMicrosoft(
    std::unique_ptr<access::ReadStorage> &storage) {
  meta_ = parseMeta(*storage);
}

LegacyMicrosoft::LegacyMicrosoft(LegacyMicrosoft &&) noexcept = default;

LegacyMicrosoft &
LegacyMicrosoft::operator=(LegacyMicrosoft &&) noexcept = default;

LegacyMicrosoft::~LegacyMicrosoft() = default;

const FileMeta &LegacyMicrosoft::meta() const noexcept { return meta_; }

bool LegacyMicrosoft::decrypted() const noexcept { return false; }

bool LegacyMicrosoft::translatable() const noexcept { return false; }

bool LegacyMicrosoft::editable() const noexcept { return false; }

bool LegacyMicrosoft::savable(const bool) const noexcept { return false; }

bool LegacyMicrosoft::decrypt(const std::string &) {
  throw UnsupportedOperation();
}

void LegacyMicrosoft::translate(const access::Path &, const Config &) {
  throw UnsupportedOperation();
}

void LegacyMicrosoft::edit(const std::string &) {
  throw UnsupportedOperation();
}

void LegacyMicrosoft::save(const access::Path &) const {
  throw UnsupportedOperation();
}

void LegacyMicrosoft::save(const access::Path &, const std::string &) const {
  throw UnsupportedOperation();
}

} // namespace oldms
} // namespace odr
