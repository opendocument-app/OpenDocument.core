#include <OpenStrategy.h>
#include <access/CfbStorage.h>
#include <access/Storage.h>
#include <access/Path.h>
#include <access/ZipStorage.h>
#include <odf/OpenDocument.h>
#include <oldms/LegacyMicrosoft.h>
#include <ooxml/OfficeOpenXml.h>
#include <odr/Exception.h>

namespace odr {

namespace {
class UnknownFile : public common::File {
public:
  const FileMeta &meta() const noexcept override {
    return {}; // TODO
  }
};
}

std::unique_ptr<common::File> OpenStrategy::openFile(const access::Path &path) {
  try {
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::ZipReader>(path);

    try {
      return std::make_unique<odf::OpenDocument>(storage);
    } catch (...) {
      // TODO
    }
    try {
      return std::make_unique<ooxml::OfficeOpenXml>(storage);
    } catch (...) {
      // TODO
    }

    // TODO return zip archive
  } catch (...) {
    // TODO
  }
  try {
    FileMeta meta;
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::CfbReader>(path);

    // legacy microsoft
    try {
      return std::make_unique<oldms::LegacyMicrosoft>(storage);
    } catch (...) {
      // TODO
    }

    // encrypted ooxml
    try {
      return std::make_unique<ooxml::OfficeOpenXml>(storage);
    } catch (...) {
      // TODO
    }

    // TODO return cfb archive
  } catch (...) {
    // TODO
  }

  return std::make_unique<UnknownFile>();
}

std::unique_ptr<common::File> OpenStrategy::openFile(const access::Path &path,
                                                     const FileType as) {
  // TODO implement
  throw UnknownFileType();
}

}
