#include <odr/internal/open_strategy.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/cfb/cfb_file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/image_file.hpp>
#include <odr/internal/csv/csv_file.hpp>
#include <odr/internal/json/json_file.hpp>
#include <odr/internal/magic.hpp>
#include <odr/internal/odf/odf_file.hpp>
#include <odr/internal/oldms/oldms_file.hpp>
#include <odr/internal/oldms_wvware/wvware_oldms_file.hpp>
#include <odr/internal/ooxml/ooxml_file.hpp>
#include <odr/internal/pdf/pdf_file.hpp>
#include <odr/internal/pdf_poppler/poppler_pdf_file.hpp>
#include <odr/internal/svm/svm_file.hpp>
#include <odr/internal/zip/zip_file.hpp>

#include <algorithm>

namespace odr::internal {

namespace {

template <typename T> auto priority_comparator(const std::vector<T> &priority) {
  return [&priority](const T &a, const T &b) {
    auto a_it = std::find(std::begin(priority), std::end(priority), a);
    auto b_it = std::find(std::begin(priority), std::end(priority), b);

    if (a_it == std::end(priority) && b_it == std::end(priority)) {
      return false;
    }
    if (a_it == std::end(priority)) {
      return false;
    }
    if (b_it == std::end(priority)) {
      return true;
    }

    return std::distance(std::begin(priority), a_it) <
           std::distance(std::begin(priority), b_it);
  };
}

} // namespace

std::vector<FileType>
open_strategy::list_file_types(const std::shared_ptr<abstract::File> &file) {
  std::vector<FileType> result;

  auto file_type = magic::file_type(*file);

  // TODO if `file` is in memory we would copy it unnecessarily
  auto memory_file = std::make_shared<common::MemoryFile>(*file);

  if (file_type == FileType::zip) {
    zip::ZipFile zip_file(memory_file);
    result.push_back(FileType::zip);

    auto filesystem = zip_file.archive()->as_filesystem();

    try {
      result.push_back(odf::OpenDocumentFile(filesystem).file_type());
    } catch (...) {
    }

    try {
      result.push_back(ooxml::OfficeOpenXmlFile(filesystem).file_type());
    } catch (...) {
    }
  } else if (file_type == FileType::compound_file_binary_format) {
    cfb::CfbFile cfb_file(memory_file);
    result.push_back(FileType::compound_file_binary_format);

    auto filesystem = cfb_file.archive()->as_filesystem();

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

    // some pdf files are weird
#ifdef ODR_WITH_PDF2HTMLEX
    try {
      result.push_back(PopplerPdfFile(memory_file).file_type());
    } catch (...) {
    }
#endif
  } else {
    result.push_back(file_type);
  }

  return result;
}

std::vector<DecoderEngine> open_strategy::list_decoder_engines(
    const std::shared_ptr<abstract::File> & /*file*/, FileType as) {
  std::vector<DecoderEngine> result;

  result.push_back(DecoderEngine::odr);

  if (as == FileType::legacy_word_document) {
    result.push_back(DecoderEngine::wvware);
  }

  if (as == FileType::portable_document_format) {
    result.push_back(DecoderEngine::poppler);
  }

  return result;
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file) {
  auto file_type = magic::file_type(*file);

  // TODO if `file` is in memory we would copy it unnecessarily
  auto memory_file = std::make_shared<common::MemoryFile>(*file);

  if (file_type == FileType::zip) {
    auto zip_file = std::make_unique<zip::ZipFile>(std::move(memory_file));

    auto filesystem = zip_file->archive()->as_filesystem();

    try {
      return std::make_unique<odf::OpenDocumentFile>(filesystem);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
    }

    return zip_file;
  } else if (file_type == FileType::compound_file_binary_format) {
    auto cfb_file = std::make_unique<cfb::CfbFile>(std::move(memory_file));

    auto filesystem = cfb_file->archive()->as_filesystem();

    try {
      return std::make_unique<oldms::LegacyMicrosoftFile>(filesystem);
    } catch (...) {
    }

    try {
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
    }

    return cfb_file;
  } else if (file_type == FileType::portable_document_format) {
    return std::make_unique<PdfFile>(file);
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

    // some pdf files are weird
#ifdef ODR_WITH_PDF2HTMLEX
    try {
      return std::make_unique<PopplerPdfFile>(memory_file);
    } catch (...) {
    }
#endif

    throw UnknownFileType();
  }

  throw UnsupportedFileType(file_type);
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file, FileType as) {
  DecodePreference preference;
  preference.as_file_type = as;
  return open_file(file, preference);
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file, FileType as,
                         DecoderEngine with) {
  if (as == FileType::opendocument_text ||
      as == FileType::opendocument_presentation ||
      as == FileType::opendocument_spreadsheet ||
      as == FileType::opendocument_graphics) {
    if (with == DecoderEngine::odr) {
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        auto zip_file = std::make_unique<zip::ZipFile>(std::move(memory_file));
        auto filesystem = zip_file->archive()->as_filesystem();
        return std::make_unique<odf::OpenDocumentFile>(filesystem);
      } catch (...) {
      }
      throw NoOpenDocumentFile();
    }
    throw NoOpenDocumentFile();
  }

  if (as == FileType::office_open_xml_document ||
      as == FileType::office_open_xml_presentation ||
      as == FileType::office_open_xml_workbook ||
      as == FileType::office_open_xml_encrypted) {
    if (with == DecoderEngine::odr) {
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        auto zip_file = std::make_unique<zip::ZipFile>(std::move(memory_file));
        auto filesystem = zip_file->archive()->as_filesystem();
        return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
      } catch (...) {
      }
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        auto cfb_file = std::make_unique<cfb::CfbFile>(std::move(memory_file));
        auto filesystem = cfb_file->archive()->as_filesystem();
        return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
      } catch (...) {
      }
      throw NoOfficeOpenXmlFile();
    }
    throw NoOfficeOpenXmlFile();
  }

  if (as == FileType::legacy_word_document ||
      as == FileType::legacy_powerpoint_presentation ||
      as == FileType::legacy_excel_worksheets) {
    if (with == DecoderEngine::odr) {
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        auto cfb_file = std::make_unique<cfb::CfbFile>(std::move(memory_file));
        auto filesystem = cfb_file->archive()->as_filesystem();
        return std::make_unique<oldms::LegacyMicrosoftFile>(filesystem);
      } catch (...) {
      }
      throw NoLegacyMicrosoftFile();
    }
#ifdef ODR_WITH_WVWARE
    if (with == DecoderEngine::wvware) {
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        return std::make_unique<odr::internal::WvWareLegacyMicrosoftFile>(
            std::move(memory_file));
      } catch (...) {
      }
      throw NoLegacyMicrosoftFile();
    }
#endif
    throw NoLegacyMicrosoftFile();
  }

  if (as == FileType::portable_document_format) {
    if (with == DecoderEngine::odr) {
      try {
        return std::make_unique<PdfFile>(file);
      } catch (...) {
      }
      throw NoPdfFile();
    }
#ifdef ODR_WITH_PDF2HTMLEX
    if (with == DecoderEngine::poppler) {
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        return std::make_unique<PopplerPdfFile>(memory_file);
      } catch (...) {
      }
      throw NoPdfFile();
    }
#endif
    throw NoPdfFile();
  }

  if (as == FileType::portable_network_graphics ||
      as == FileType::graphics_interchange_format || as == FileType::jpeg ||
      as == FileType::bitmap_image_file) {
    if (with == DecoderEngine::odr) {
      try {
        return std::make_unique<common::ImageFile>(file, as);
      } catch (...) {
      }
      throw NoImageFile();
    }
    throw NoImageFile();
  }

  if (as == FileType::starview_metafile) {
    if (with == DecoderEngine::odr) {
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        return std::make_unique<svm::SvmFile>(memory_file);
      } catch (...) {
      }
      throw NoSvmFile();
    }
    throw NoSvmFile();
  }

  if (as == FileType::text_file) {
    if (with == DecoderEngine::odr) {
      try {
        return std::make_unique<text::TextFile>(file);
      } catch (...) {
      }
      throw NoTextFile();
    }
    throw NoTextFile();
  }

  if (as == FileType::comma_separated_values) {
    if (with == DecoderEngine::odr) {
      try {
        auto text = std::make_shared<text::TextFile>(file);
        return std::make_unique<csv::CsvFile>(text);
      } catch (...) {
      }
      throw NoCsvFile();
    }
    throw NoCsvFile();
  }

  if (as == FileType::javascript_object_notation) {
    if (with == DecoderEngine::odr) {
      try {
        auto text = std::make_shared<text::TextFile>(file);
        return std::make_unique<json::JsonFile>(text);
      } catch (...) {
      }
      throw NoJsonFile();
    }
    throw NoJsonFile();
  }

  if (as == FileType::zip) {
    if (with == DecoderEngine::odr) {
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        return std::make_unique<zip::ZipFile>(memory_file);
      } catch (...) {
      }
      throw NoZipFile();
    }
    throw NoZipFile();
  }

  if (as == FileType::compound_file_binary_format) {
    if (with == DecoderEngine::odr) {
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        return std::make_unique<cfb::CfbFile>(memory_file);
      } catch (...) {
      }
      throw NoCfbFile();
    }
    throw NoCfbFile();
  }

  throw UnsupportedFileType(as);
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file,
                         const DecodePreference &preference) {
  std::vector<FileType> probe_types;
  if (preference.as_file_type.has_value()) {
    probe_types.push_back(*preference.as_file_type);
  } else {
    std::vector<FileType> detected_types = list_file_types(file);
    probe_types.insert(probe_types.end(), detected_types.begin(),
                       detected_types.end());
    auto probe_types_end = std::unique(probe_types.begin(), probe_types.end());
    probe_types.erase(probe_types_end, probe_types.end());
    // more specific file types are further down the list, so we bring them up
    std::ranges::reverse(probe_types);
  }

  std::ranges::stable_sort(probe_types,
                           priority_comparator(preference.file_type_priority));

  for (FileType as : probe_types) {
    std::vector<DecoderEngine> probe_engines;
    if (preference.with_engine.has_value()) {
      probe_engines.push_back(*preference.with_engine);
    } else {
      std::vector<DecoderEngine> detected_engines =
          list_decoder_engines(file, as);
      probe_engines.insert(probe_engines.end(), detected_engines.begin(),
                           detected_engines.end());
      auto probe_engines_end =
          std::unique(probe_engines.begin(), probe_engines.end());
      probe_engines.erase(probe_engines_end, probe_engines.end());
    }

    std::ranges::stable_sort(probe_engines,
                             priority_comparator(preference.engine_priority));

    for (DecoderEngine with : probe_engines) {
      auto decoded_file = open_file(file, as, with);
      if (decoded_file != nullptr) {
        return decoded_file;
      }
    }
  }

  throw UnknownFileType();
}

std::unique_ptr<abstract::DocumentFile>
open_strategy::open_document_file(std::shared_ptr<abstract::File> file) {
  auto file_type = magic::file_type(*file);

  if (file_type == FileType::zip) {
    // TODO if `file` is in memory we would copy it unnecessarily
    auto memory_file = std::make_shared<common::MemoryFile>(*file);

    auto zip_file = std::make_unique<zip::ZipFile>(std::move(memory_file));

    auto filesystem = zip_file->archive()->as_filesystem();

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

    auto cfb_file = std::make_unique<cfb::CfbFile>(std::move(memory_file));

    auto filesystem = cfb_file->archive()->as_filesystem();

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
