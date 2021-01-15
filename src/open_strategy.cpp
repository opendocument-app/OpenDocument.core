#include <cfb/cfb_storage.h>
#include <common/file.h>
#include <common/path.h>
#include <odf/odf_document_file.h>
#include <odr/exceptions.h>
#include <oldms/oldms_document_file.h>
#include <ooxml/ooxml_document_file.h>
#include <open_strategy.h>
#include <zip/zip_storage.h>

namespace odr {

std::vector<FileType> open_strategy::types(const common::Path &path) {
  std::vector<FileType> result;

  // TODO throw if not a file

  try {
    auto storage = std::make_shared<zip::ZipReader>(path);
    result.push_back(FileType::ZIP);

    try {
      result.push_back(odf::OpenDocumentFile(storage).file_type());
    } catch (...) {
    }

    try {
      result.push_back(ooxml::OfficeOpenXmlFile(storage).file_type());
    } catch (...) {
    }
  } catch (...) {
  }

  try {
    FileMeta meta;
    auto storage = std::make_shared<cfb::CfbReader>(path);
    result.push_back(FileType::COMPOUND_FILE_BINARY_FORMAT);

    // legacy microsoft
    try {
      result.push_back(oldms::LegacyMicrosoftFile(storage).file_type());
    } catch (...) {
    }

    // encrypted ooxml
    try {
      result.push_back(ooxml::OfficeOpenXmlFile(storage).file_type());
    } catch (...) {
    }
  } catch (...) {
  }

  return result;
}

std::unique_ptr<abstract::File>
open_strategy::open_file(const common::Path &path) {
  // TODO throw if not a file

  try {
    auto storage = std::make_shared<zip::ZipReader>(path);

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
    auto storage = std::make_shared<cfb::CfbReader>(path);

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

  return std::make_unique<common::DiscFile>(path);
}

std::unique_ptr<abstract::File>
open_strategy::open_file(const common::Path &path, const FileType as) {
  // TODO implement
  throw UnknownFileType();
}

std::unique_ptr<abstract::DocumentFile>
open_strategy::open_document_file(const common::Path &path) {
  try {
    auto storage = std::make_shared<zip::ZipReader>(path);

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
    auto storage = std::make_shared<cfb::CfbReader>(path);

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
