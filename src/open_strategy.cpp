#include <internal/abstract/archive.h>
#include <internal/cfb/cfb_archive.h>
#include <internal/common/archive.h>
#include <internal/common/file.h>
#include <internal/common/image_file.h>
#include <internal/csv/csv_file.h>
#include <internal/json/json_file.h>
#include <internal/odf/odf_file.h>
#include <internal/oldms/oldms_file.h>
#include <internal/ooxml/ooxml_file.h>
#include <internal/svm/svm_file.h>
#include <internal/text/text_file.h>
#include <internal/zip/zip_archive.h>
#include <magic.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <open_strategy.h>
#include <utility>

using namespace odr::internal;

namespace odr {

std::vector<FileType>
open_strategy::types(std::shared_ptr<abstract::File> file) {
  std::vector<FileType> result;

  auto file_type = magic::file_type(*file);

  // TODO if `file` is in memory we would copy it unnecessarily
  auto memory_file = std::make_shared<common::MemoryFile>(*file);

  if (file_type == FileType::zip) {
    auto zip = common::ArchiveFile(zip::ReadonlyZipArchive(memory_file));
    result.push_back(FileType::zip);

    auto filesystem = zip.archive()->filesystem();

    try {
      result.push_back(odf::OpenDocumentFile(filesystem).file_type());
    } catch (...) {
    }

    try {
      result.push_back(ooxml::OfficeOpenXmlFile(filesystem).file_type());
    } catch (...) {
    }
  } else if (file_type == FileType::compound_file_binary_format) {
    auto cfb = common::ArchiveFile(cfb::ReadonlyCfbArchive(memory_file));
    result.push_back(FileType::compound_file_binary_format);

    auto filesystem = cfb.archive()->filesystem();

    try {
      result.push_back(oldms::LegacyMicrosoftFile(filesystem).file_type());
    } catch (...) {
    }

    try {
      result.push_back(ooxml::OfficeOpenXmlFile(filesystem).file_type());
    } catch (...) {
    }
  } else if (file_type == FileType::portable_network_graphics ||
             file_type == FileType::graphics_interchange_format ||
             file_type == FileType::jpeg ||
             file_type == FileType::bitmap_image_file) {
    result.push_back(file_type);
  } else if (file_type == FileType::starview_metafile) {
    try {
      result.push_back(svm::SvmFile(memory_file).file_type());
    } catch (...) {
    }
  }

  try {
    auto text = std::make_shared<text::TextFile>(file);
    result.push_back(FileType::text_file);

    try {
      result.push_back(csv::CsvFile(text).file_type());
    } catch (...) {
    }

    try {
      result.push_back(json::JsonFile(text).file_type());
    } catch (...) {
    }
  } catch (...) {
  }

  return result;
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file) {
  auto file_type = magic::file_type(*file);

  // TODO if `file` is in memory we would copy it unnecessarily
  auto memory_file = std::make_shared<common::MemoryFile>(*file);

  if (file_type == FileType::zip) {
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
  } else if (file_type == FileType::compound_file_binary_format) {
    auto cfb = std::make_unique<common::ArchiveFile<cfb::ReadonlyCfbArchive>>(
        cfb::ReadonlyCfbArchive(std::move(memory_file)));

    auto filesystem = cfb->archive()->filesystem();

    try {
      return std::make_unique<oldms::LegacyMicrosoftFile>(filesystem);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
    }

    return cfb;
  } else if (file_type == FileType::portable_network_graphics ||
             file_type == FileType::graphics_interchange_format ||
             file_type == FileType::jpeg ||
             file_type == FileType::bitmap_image_file) {
    return std::make_unique<common::ImageFile>(file, file_type);
  } else if (file_type == FileType::starview_metafile) {
    return std::make_unique<svm::SvmFile>(memory_file);
  } else if (file_type == FileType::unknown) {
    try {
      auto text = std::make_shared<text::TextFile>(file);

      try {
        return std::make_unique<csv::CsvFile>(text);
      } catch (...) {
      }

      try {
        return std::make_unique<json::JsonFile>(text);
      } catch (...) {
      }

      // TODO looks dirty
      return std::make_unique<text::TextFile>(file);
    } catch (...) {
    }

    throw UnknownFileType();
  }

  throw UnsupportedFileType(file_type);
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> /*file*/,
                         const FileType /*as*/) {
  // TODO implement
  throw UnknownFileType();
}

std::unique_ptr<abstract::DocumentFile>
open_strategy::open_document_file(std::shared_ptr<abstract::File> file) {
  auto file_type = magic::file_type(*file);

  if (file_type == FileType::zip) {
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
  } else if (file_type == FileType::compound_file_binary_format) {
    // TODO if `file` is in memory we would copy it unnecessarily
    auto memory_file = std::make_unique<common::MemoryFile>(*file);

    auto cfb = std::make_unique<common::ArchiveFile<cfb::ReadonlyCfbArchive>>(
        cfb::ReadonlyCfbArchive(std::move(memory_file)));

    auto filesystem = cfb->archive()->filesystem();

    try {
      return std::make_unique<oldms::LegacyMicrosoftFile>(filesystem);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
    }
  }

  throw NoDocumentFile();
}

} // namespace odr
