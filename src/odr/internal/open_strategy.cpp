#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/cfb/cfb_archive.hpp>
#include <odr/internal/common/archive.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/image_file.hpp>
#include <odr/internal/csv/csv_file.hpp>
#include <odr/internal/json/json_file.hpp>
#include <odr/internal/magic.hpp>
#include <odr/internal/odf/odf_file.hpp>
#include <odr/internal/oldms/oldms_file.hpp>
#include <odr/internal/ooxml/ooxml_file.hpp>
#include <odr/internal/open_strategy.hpp>
#include <odr/internal/svm/svm_file.hpp>
#include <odr/internal/text/text_file.hpp>
#include <odr/internal/zip/zip_archive.hpp>
#include <utility>

namespace odr::internal {

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
  } else if (file_type == FileType::unknown) {
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
  } else {
    result.push_back(file_type);
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

} // namespace odr::internal
