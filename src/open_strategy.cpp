#include <access/cfb_storage.h>
#include <access/path.h>
#include <access/storage.h>
#include <access/zip_storage.h>
#include <odf/odf_document_file.h>
#include <odr/exceptions.h>
#include <oldms/oldms_document_file.h>
#include <ooxml/ooxml_document_file.h>
#include <open_strategy.h>

namespace odr {

namespace {
class UnknownFile final : public common::File {
public:
  explicit UnknownFile(const access::Path &path) {
    // TODO check if file exists
  }

  FileType fileType() const noexcept final { return FileType::UNKNOWN; }

  FileMeta fileMeta() const noexcept final { return {}; }

  std::unique_ptr<std::istream> data() const final { return {}; }
};
} // namespace

std::vector<FileType> OpenStrategy::types(const access::Path &path) {
  std::vector<FileType> result;

  // TODO throw if not a file

  try {
    auto storage = std::make_shared<access::ZipReader>(path);
    result.push_back(FileType::ZIP);

    try {
      result.push_back(odf::OpenDocumentFile(storage).fileType());
    } catch (...) {
    }

    try {
      result.push_back(ooxml::OfficeOpenXmlFile(storage).fileType());
    } catch (...) {
    }
  } catch (...) {
  }

  try {
    FileMeta meta;
    auto storage = std::make_shared<access::CfbReader>(path);
    result.push_back(FileType::COMPOUND_FILE_BINARY_FORMAT);

    // legacy microsoft
    try {
      result.push_back(oldms::LegacyMicrosoftFile(storage).fileType());
    } catch (...) {
    }

    // encrypted ooxml
    try {
      result.push_back(ooxml::OfficeOpenXmlFile(storage).fileType());
    } catch (...) {
    }
  } catch (...) {
  }

  return result;
}

std::unique_ptr<common::File> OpenStrategy::openFile(const access::Path &path) {
  // TODO throw if not a file

  try {
    auto storage = std::make_shared<access::ZipReader>(path);

    try {
      return std::make_unique<odf::OpenDocumentFile>(storage);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(storage);
    } catch (...) {
    }

    // TODO return zip archive file
  } catch (...) {
  }

  try {
    auto storage = std::make_shared<access::CfbReader>(path);

    // legacy microsoft
    try {
      return std::make_unique<oldms::LegacyMicrosoftFile>(storage);
    } catch (...) {
    }

    // encrypted ooxml
    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(storage);
    } catch (...) {
    }

    // TODO return cfb archive file
  } catch (...) {
  }

  return std::make_unique<UnknownFile>(path);
}

std::unique_ptr<common::File> OpenStrategy::openFile(const access::Path &path,
                                                     const FileType as) {
  // TODO implement
  throw UnknownFileType();
}

std::unique_ptr<common::DocumentFile>
OpenStrategy::openDocumentFile(const access::Path &path) {
  try {
    auto storage = std::make_shared<access::ZipReader>(path);

    try {
      return std::make_unique<odf::OpenDocumentFile>(storage);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(storage);
    } catch (...) {
    }
  } catch (...) {
  }

  try {
    auto storage = std::make_shared<access::CfbReader>(path);

    // legacy microsoft
    try {
      return std::make_unique<oldms::LegacyMicrosoftFile>(storage);
    } catch (...) {
    }

    // encrypted ooxml
    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(storage);
    } catch (...) {
    }
  } catch (...) {
  }

  // TODO throw
  return {};
}

} // namespace odr
