#include <cfb/cfb_archive.h>
#include <common/file.h>
#include <common/path.h>
#include <common/filesystem.h>
#include <odf/odf_document_file.h>
#include <odr/exceptions.h>
#include <oldms/oldms_document_file.h>
#include <ooxml/ooxml_document_file.h>
#include <open_strategy.h>
#include <zip/zip_archive.h>

namespace odr {

std::vector<FileType>
open_strategy::types(std::shared_ptr<abstract::File> file) {
  std::vector<FileType> result;

  try {
    auto zip = std::make_shared<zip::ReadonlyZipArchive>(file);
    result.push_back(FileType::ZIP);

    // TODO
    auto filesystem = std::make_shared<common::VirtualFilesystem>();

    try {
      result.push_back(odf::OpenDocumentFile(filesystem).file_type());
    } catch (...) {
    }

    try {
      result.push_back(ooxml::OfficeOpenXmlFile(filesystem).file_type());
    } catch (...) {
    }
  } catch (...) {
  }

  try {
    auto memory_file = std::make_shared<common::MemoryFile>(*file);

    auto cfb = std::make_shared<cfb::ReadonlyCfbArchive>(memory_file);
    result.push_back(FileType::COMPOUND_FILE_BINARY_FORMAT);

    // TODO
    auto filesystem = std::make_shared<common::VirtualFilesystem>();

    // legacy microsoft
    try {
      result.push_back(oldms::LegacyMicrosoftFile(filesystem).file_type());
    } catch (...) {
    }

    // encrypted ooxml
    try {
      result.push_back(ooxml::OfficeOpenXmlFile(filesystem).file_type());
    } catch (...) {
    }
  } catch (...) {
  }

  return result;
}

std::shared_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file) {
  // TODO throw if not a file

  try {
    auto zip = std::make_shared<zip::ReadonlyZipArchive>(file);

    // TODO
    auto filesystem = std::make_shared<common::VirtualFilesystem>();

    try {
      return std::make_unique<odf::OpenDocumentFile>(filesystem);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
    }

    // TODO return zip archive file
  } catch (...) {
  }

  try {
    auto memory_file = std::make_shared<common::MemoryFile>(*file);

    auto cfb = std::make_shared<cfb::ReadonlyCfbArchive>(memory_file);

    // TODO
    auto filesystem = std::make_shared<common::VirtualFilesystem>();

    // legacy microsoft
    try {
      return std::make_unique<oldms::LegacyMicrosoftFile>(filesystem);
    } catch (...) {
    }

    // encrypted ooxml
    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
    }

    // TODO return cfb archive file
  } catch (...) {
  }

  return {};
}

std::shared_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file,
                         const FileType as) {
  // TODO implement
  throw UnknownFileType();
}

std::unique_ptr<abstract::DocumentFile>
open_strategy::open_document_file(std::shared_ptr<abstract::File> file) {
  try {
    auto zip = std::make_shared<zip::ReadonlyZipArchive>(file);

    // TODO
    auto filesystem = std::make_shared<common::VirtualFilesystem>();

    try {
      return std::make_unique<odf::OpenDocumentFile>(filesystem);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
    }
  } catch (...) {
  }

  try {
    auto memory_file = std::make_shared<common::MemoryFile>(*file);

    auto cfb = std::make_shared<cfb::ReadonlyCfbArchive>(memory_file);

    // TODO
    auto filesystem = std::make_shared<common::VirtualFilesystem>();

    // legacy microsoft
    try {
      return std::make_unique<oldms::LegacyMicrosoftFile>(filesystem);
    } catch (...) {
    }

    // encrypted ooxml
    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
    }
  } catch (...) {
  }

  // TODO throw
  return {};
}

} // namespace odr
