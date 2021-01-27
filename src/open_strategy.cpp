#include <cfb/cfb_archive.h>
#include <common/file.h>
#include <common/path.h>
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

  // TODO throw if not a file

  try {
    auto zip_archive = std::make_shared<zip::ZipArchive>(file);
    result.push_back(FileType::ZIP);

    try {
      result.push_back(odf::OpenDocumentFile(zip_archive).file_type());
    } catch (...) {
    }

    try {
      result.push_back(ooxml::OfficeOpenXmlFile(zip_archive).file_type());
    } catch (...) {
    }
  } catch (...) {
  }

  try {
    FileMeta meta;
    auto cfb_file = std::make_shared<cfb::ReadonlyCfbArchive>(file);
    result.push_back(FileType::COMPOUND_FILE_BINARY_FORMAT);

    // legacy microsoft
    try {
      result.push_back(oldms::LegacyMicrosoftFile(cfb_file).file_type());
    } catch (...) {
    }

    // encrypted ooxml
    try {
      result.push_back(ooxml::OfficeOpenXmlFile(cfb_file).file_type());
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
    auto zip_file = std::make_shared<zip::ZipFile>(file);

    try {
      return std::make_unique<odf::OpenDocumentFile>(zip_file);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(zip_file);
    } catch (...) {
    }

    // TODO return zip archive file
  } catch (...) {
  }

  try {
    auto cfb_file = std::make_shared<cfb::CfbFile>(file);

    // legacy microsoft
    try {
      return std::make_unique<oldms::LegacyMicrosoftFile>(cfb_file);
    } catch (...) {
    }

    // encrypted ooxml
    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(cfb_file);
    } catch (...) {
    }

    // TODO return cfb archive file
  } catch (...) {
  }

  return file;
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
    auto zip_file = std::make_shared<zip::ZipFile>(file);

    try {
      return std::make_unique<odf::OpenDocumentFile>(zip_file);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(zip_file);
    } catch (...) {
    }
  } catch (...) {
  }

  try {
    auto cfb_file = std::make_shared<cfb::CfbFile>(file);

    // legacy microsoft
    try {
      return std::make_unique<oldms::LegacyMicrosoftFile>(cfb_file);
    } catch (...) {
    }

    // encrypted ooxml
    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(cfb_file);
    } catch (...) {
    }
  } catch (...) {
  }

  // TODO throw
  return {};
}

} // namespace odr
