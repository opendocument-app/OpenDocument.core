#include <OpenStrategy.h>
#include <access/CfbStorage.h>
#include <access/Path.h>
#include <access/Storage.h>
#include <access/ZipStorage.h>
#include <odf/OpenDocument.h>
#include <odr/Exception.h>
#include <oldms/LegacyMicrosoftFile.h>
#include <ooxml/OfficeOpenXml.h>

namespace odr {

namespace {
class UnknownFile final : public common::File {
public:
  FileType fileType() const noexcept final {
    return FileType::UNKNOWN;
  }

  FileMeta fileMeta() const noexcept final {
    return {};
  }
};
}

std::vector<FileType> OpenStrategy::types(const access::Path &path) {
  std::vector<FileType> result;

  // TODO throw if not a file
  // TODO add unknown?

  try {
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::ZipReader>(path);
    result.push_back(FileType::ZIP);

    try {
      // TODO
      //result.push_back(odf::OpenDocument(storage).meta().type);
    } catch (...) {}

    try {
      // TODO
      //result.push_back(ooxml::OfficeOpenXml(storage).meta().type);
    } catch (...) {}
  } catch (...) {}

  try {
    FileMeta meta;
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::CfbReader>(path);
    result.push_back(FileType::COMPOUND_FILE_BINARY_FORMAT);

    // legacy microsoft
    try {
      // TODO
      //result.push_back(oldms::LegacyMicrosoftFile(storage).meta().type);
    } catch (...) {}

    // encrypted ooxml
    try {
      // TODO
      //result.push_back(ooxml::OfficeOpenXml(storage).meta().type);
    } catch (...) {}
  } catch (...) {}

  return result;
}

std::unique_ptr<common::File> OpenStrategy::openFile(const access::Path &path) {
  // TODO throw if not a file

  try {
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::ZipReader>(path);

    try {
      // TODO
      //return std::make_unique<odf::OpenDocument>(storage);
    } catch (...) {}

    try {
      // TODO
      //return std::make_unique<ooxml::OfficeOpenXml>(storage);
    } catch (...) {}

    // TODO return zip archive
  } catch (...) {}

  try {
    FileMeta meta;
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::CfbReader>(path);

    // legacy microsoft
    try {
      // TODO
      //return std::make_unique<oldms::LegacyMicrosoftFile>(storage);
    } catch (...) {}

    // encrypted ooxml
    try {
      // TODO
      //return std::make_unique<ooxml::OfficeOpenXml>(storage);
    } catch (...) {}

    // TODO return cfb archive
  } catch (...) {}

  return std::make_unique<UnknownFile>();
}

std::unique_ptr<common::File> OpenStrategy::openFile(const access::Path &path,
                                                     const FileType as) {
  // TODO implement
  throw UnknownFileType();
}

std::unique_ptr<common::Document> OpenStrategy::openDocument(const access::Path &path) {
  try {
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::ZipReader>(path);

    try {
      // TODO
      //return std::make_unique<odf::OpenDocument>(storage);
    } catch (...) {}

    try {
      // TODO
      //return std::make_unique<ooxml::OfficeOpenXml>(storage);
    } catch (...) {}
  } catch (...) {}

  try {
    FileMeta meta;
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::CfbReader>(path);

    // legacy microsoft
    try {
      // TODO
      //return std::make_unique<oldms::LegacyMicrosoftFile>(storage);
    } catch (...) {}

    // encrypted ooxml
    try {
      // TODO
      //return std::make_unique<ooxml::OfficeOpenXml>(storage);
    } catch (...) {}
  } catch (...) {}
}

std::unique_ptr<common::Document> OpenStrategy::openDocument(const access::Path &path, FileType as) {
  // TODO implement
  throw UnknownFileType();
}

}
