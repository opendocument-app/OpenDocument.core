#include <internal/cfb/cfb_archive.h>
#include <internal/common/archive.h>
#include <internal/common/file.h>
#include <internal/odf/odf_file.h>
#include <internal/oldms/oldms_file.h>
#include <internal/ooxml/ooxml_file.h>
#include <internal/zip/zip_archive.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <open_strategy.h>

using namespace odr::internal;

namespace odr {

std::vector<FileType>
open_strategy::types(std::shared_ptr<abstract::File> file) {
  std::vector<FileType> result;

  try {
    // TODO if `file` is in memory we would copy it unnecessarily
    auto memory_file = std::make_shared<common::MemoryFile>(*file);

    auto zip = common::ArchiveFile(zip::ReadonlyZipArchive(memory_file));
    result.push_back(FileType::ZIP);

    auto filesystem = zip.archive()->filesystem();

    try {
      result.push_back(odf::OpenDocumentFile(filesystem).file_type());
    } catch (...) {
    }

    try {
      result.push_back(ooxml::OfficeOpenXmlFile(filesystem).file_type());
    } catch (...) {
    }

    return result;
  } catch (...) {
  }

  try {
    // TODO if `file` is in memory we would copy it unnecessarily
    auto memory_file = std::make_shared<common::MemoryFile>(*file);

    auto cfb = common::ArchiveFile(cfb::ReadonlyCfbArchive(memory_file));
    result.push_back(FileType::COMPOUND_FILE_BINARY_FORMAT);

    auto filesystem = cfb.archive()->filesystem();

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

    return result;
  } catch (...) {
  }

  return result;
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file) {
  try {
    // TODO if `file` is in memory we would copy it unnecessarily
    auto memory_file = std::make_shared<common::MemoryFile>(*file);

    auto zip = std::make_unique<common::ArchiveFile<zip::ReadonlyZipArchive>>(
        zip::ReadonlyZipArchive(memory_file));

    auto filesystem = zip->archive()->filesystem();

    try {
      return std::make_unique<odf::OpenDocumentFile>(filesystem);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
    }

    return zip;
  } catch (...) {
  }

  try {
    // TODO if `file` is in memory we would copy it unnecessarily
    auto memory_file = std::make_unique<common::MemoryFile>(*file);

    auto cfb = std::make_unique<common::ArchiveFile<cfb::ReadonlyCfbArchive>>(
        cfb::ReadonlyCfbArchive(std::move(memory_file)));

    auto filesystem = cfb->archive()->filesystem();

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

    return cfb;
  } catch (...) {
  }

  return {};
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file,
                         const FileType as) {
  // TODO implement
  throw UnknownFileType();
}

std::unique_ptr<abstract::DocumentFile>
open_strategy::open_document_file(std::shared_ptr<abstract::File> file) {
  try {
    // TODO if `file` is in memory we would copy it unnecessarily
    auto memory_file = std::make_shared<common::MemoryFile>(*file);

    auto zip = std::make_unique<common::ArchiveFile<zip::ReadonlyZipArchive>>(
        zip::ReadonlyZipArchive(memory_file));

    auto filesystem = zip->archive()->filesystem();

    try {
      return std::make_unique<odf::OpenDocumentFile>(filesystem);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
    }

    return {};
  } catch (...) {
  }

  try {
    // TODO if `file` is in memory we would copy it unnecessarily
    auto memory_file = std::make_shared<common::MemoryFile>(*file);

    auto cfb = std::make_unique<common::ArchiveFile<cfb::ReadonlyCfbArchive>>(
        cfb::ReadonlyCfbArchive(std::move(memory_file)));

    auto filesystem = cfb->archive()->filesystem();

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

    return {};
  } catch (...) {
  }

  // TODO throw
  return {};
}

} // namespace odr
