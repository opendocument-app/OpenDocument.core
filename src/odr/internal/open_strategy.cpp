#include <odr/internal/open_strategy.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

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
open_strategy::list_file_types(const std::shared_ptr<abstract::File> &file,
                               Logger &logger) {
  std::vector<FileType> result;

  auto file_type = magic::file_type(*file);
  ODR_VERBOSE(logger,
              "magic determined file type " << file_type_to_string(file_type));

  // TODO if `file` is in memory we would copy it unnecessarily
  auto memory_file = std::make_shared<common::MemoryFile>(*file);

  if (file_type == FileType::zip) {
    ODR_VERBOSE(logger, "open as zip");

    zip::ZipFile zip_file(memory_file);
    result.push_back(FileType::zip);

    auto filesystem = zip_file.archive()->as_filesystem();

    try {
      ODR_VERBOSE(logger, "try open as odf");
      result.push_back(odf::OpenDocumentFile(filesystem).file_type());
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as odf");
    }

    try {
      ODR_VERBOSE(logger, "try open as ooxml");
      result.push_back(ooxml::OfficeOpenXmlFile(filesystem).file_type());
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as ooxml");
    }
  } else if (file_type == FileType::compound_file_binary_format) {
    ODR_VERBOSE(logger, "open as cbf");

    cfb::CfbFile cfb_file(memory_file);
    result.push_back(FileType::compound_file_binary_format);

    auto filesystem = cfb_file.archive()->as_filesystem();

    try {
      ODR_VERBOSE(logger, "try open as legacy ms");
      result.push_back(oldms::LegacyMicrosoftFile(filesystem).file_type());
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as legacy ms");
    }

    try {
      ODR_VERBOSE(logger, "try open as ooxml");
      result.push_back(ooxml::OfficeOpenXmlFile(filesystem).file_type());
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as ooxml");
    }
  } else if (file_type == FileType::starview_metafile) {
    try {
      ODR_VERBOSE(logger, "try open as svm");
      result.push_back(svm::SvmFile(memory_file).file_type());
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as svm");
    }
  } else if (file_type == FileType::unknown) {
    try {
      ODR_VERBOSE(logger, "try open as text");

      auto text = std::make_shared<text::TextFile>(file);
      result.push_back(FileType::text_file);

      try {
        ODR_VERBOSE(logger, "try open as csv");
        result.push_back(csv::CsvFile(text).file_type());
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as csv");
      }

      try {
        ODR_VERBOSE(logger, "try open as json");
        result.push_back(json::JsonFile(text).file_type());
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as json");
      }
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as text");
    }

    // some pdf files are weird
#ifdef ODR_WITH_PDF2HTMLEX
    try {
      ODR_VERBOSE(logger, "try open as pdf with poppler");
      result.push_back(PopplerPdfFile(memory_file).file_type());
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as pdf with poppler");
    }
#endif
  } else {
    ODR_VERBOSE(logger, "anything else");
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
open_strategy::open_file(std::shared_ptr<abstract::File> file, Logger &logger) {
  auto file_type = magic::file_type(*file);
  ODR_VERBOSE(logger,
              "magic determined file type " << file_type_to_string(file_type));

  // TODO if `file` is in memory we would copy it unnecessarily
  auto memory_file = std::make_shared<common::MemoryFile>(*file);

  if (file_type == FileType::zip) {
    ODR_VERBOSE(logger, "open as zip");

    auto zip_file = std::make_unique<zip::ZipFile>(std::move(memory_file));

    auto filesystem = zip_file->archive()->as_filesystem();

    try {
      ODR_VERBOSE(logger, "try open as odf");
      return std::make_unique<odf::OpenDocumentFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as odf");
    }

    try {
      ODR_VERBOSE(logger, "try open as ooxml");
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as ooxml");
    }

    return zip_file;
  } else if (file_type == FileType::compound_file_binary_format) {
    ODR_VERBOSE(logger, "open as cbf");

    auto cfb_file = std::make_unique<cfb::CfbFile>(std::move(memory_file));

    auto filesystem = cfb_file->archive()->as_filesystem();

    try {
      ODR_VERBOSE(logger, "try open as legacy ms");
      return std::make_unique<oldms::LegacyMicrosoftFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as legacy ms");
    }

    try {
      ODR_VERBOSE(logger, "try open as ooxml");
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as ooxml");
    }

    return cfb_file;
  } else if (file_type == FileType::portable_document_format) {
    ODR_VERBOSE(logger, "open as pdf");
    return std::make_unique<PdfFile>(file);
  } else if (file_type == FileType::portable_network_graphics ||
             file_type == FileType::graphics_interchange_format ||
             file_type == FileType::jpeg ||
             file_type == FileType::bitmap_image_file) {
    ODR_VERBOSE(logger, "open as image");
    return std::make_unique<common::ImageFile>(file, file_type);
  } else if (file_type == FileType::starview_metafile) {
    ODR_VERBOSE(logger, "open as svm");
    return std::make_unique<svm::SvmFile>(memory_file);
  } else if (file_type == FileType::unknown) {
    ODR_VERBOSE(logger, "handle unknown file type");

    try {
      ODR_VERBOSE(logger, "try open as text");

      auto text = std::make_shared<text::TextFile>(file);

      try {
        ODR_VERBOSE(logger, "try open as csv");
        return std::make_unique<csv::CsvFile>(text);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as csv");
      }

      try {
        ODR_VERBOSE(logger, "try open as json");
        return std::make_unique<json::JsonFile>(text);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as json");
      }

      ODR_VERBOSE(logger, "open as text file");
      // TODO looks dirty
      return std::make_unique<text::TextFile>(file);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as text");
    }

    // some pdf files are weird
#ifdef ODR_WITH_PDF2HTMLEX
    try {
      ODR_VERBOSE(logger, "try open as pdf with poppler");
      return std::make_unique<PopplerPdfFile>(memory_file);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as pdf with poppler");
    }
#endif

    ODR_VERBOSE(logger, "unknown file type");
    throw UnknownFileType();
  }

  ODR_VERBOSE(logger, "unsupported file type");
  throw UnsupportedFileType(file_type);
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file, FileType as,
                         Logger &logger) {
  DecodePreference preference;
  preference.as_file_type = as;
  return open_file(file, preference, logger);
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file, FileType as,
                         DecoderEngine with, Logger &logger) {
  if (as == FileType::opendocument_text ||
      as == FileType::opendocument_presentation ||
      as == FileType::opendocument_spreadsheet ||
      as == FileType::opendocument_graphics) {
    ODR_VERBOSE(logger, "open as odf");
    if (with == DecoderEngine::odr) {
      ODR_VERBOSE(logger, "using odr engine");
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        auto zip_file = std::make_unique<zip::ZipFile>(std::move(memory_file));
        auto filesystem = zip_file->archive()->as_filesystem();
        return std::make_unique<odf::OpenDocumentFile>(filesystem);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as odf with odr engine");
      }
      throw NoOpenDocumentFile();
    }
    ODR_VERBOSE(logger, "unsupported decoder engine for odf "
                            << decoder_engine_to_string(with));
    throw UnsupportedDecoderEngine(with);
  }

  if (as == FileType::office_open_xml_document ||
      as == FileType::office_open_xml_presentation ||
      as == FileType::office_open_xml_workbook ||
      as == FileType::office_open_xml_encrypted) {
    ODR_VERBOSE(logger, "open as ooxml");
    if (with == DecoderEngine::odr) {
      ODR_VERBOSE(logger, "using odr engine");
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        auto zip_file = std::make_unique<zip::ZipFile>(std::move(memory_file));
        auto filesystem = zip_file->archive()->as_filesystem();
        return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as ooxml zip with odr engine");
      }
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        auto cfb_file = std::make_unique<cfb::CfbFile>(std::move(memory_file));
        auto filesystem = cfb_file->archive()->as_filesystem();
        return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as ooxml cfb with odr engine");
      }
      throw NoOfficeOpenXmlFile();
    }
    ODR_VERBOSE(logger, "unsupported decoder engine for ooxml "
                            << decoder_engine_to_string(with));
    throw UnsupportedDecoderEngine(with);
  }

  if (as == FileType::legacy_word_document ||
      as == FileType::legacy_powerpoint_presentation ||
      as == FileType::legacy_excel_worksheets) {
    ODR_VERBOSE(logger, "open as legacy ms");
    if (with == DecoderEngine::odr) {
      ODR_VERBOSE(logger, "using odr engine");
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        auto cfb_file = std::make_unique<cfb::CfbFile>(std::move(memory_file));
        auto filesystem = cfb_file->archive()->as_filesystem();
        return std::make_unique<oldms::LegacyMicrosoftFile>(filesystem);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as legacy ms with odr engine");
      }
      throw NoLegacyMicrosoftFile();
    }
#ifdef ODR_WITH_WVWARE
    if (with == DecoderEngine::wvware) {
      ODR_VERBOSE(logger, "using wvware engine");
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        return std::make_unique<odr::internal::WvWareLegacyMicrosoftFile>(
            std::move(memory_file));
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as legacy ms with wvware engine");
      }
      throw NoLegacyMicrosoftFile();
    }
#endif
    ODR_VERBOSE(logger, "unsupported decoder engine for legacy ms "
                            << decoder_engine_to_string(with));
    throw UnsupportedDecoderEngine(with);
  }

  if (as == FileType::portable_document_format) {
    ODR_VERBOSE(logger, "open as pdf");
    if (with == DecoderEngine::odr) {
      ODR_VERBOSE(logger, "using odr engine");
      try {
        return std::make_unique<PdfFile>(file);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as pdf with odr engine");
      }
      throw NoPdfFile();
    }
#ifdef ODR_WITH_PDF2HTMLEX
    if (with == DecoderEngine::poppler) {
      ODR_VERBOSE(logger, "using poppler engine");
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        return std::make_unique<PopplerPdfFile>(memory_file);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as pdf with poppler engine");
      }
      throw NoPdfFile();
    }
#endif
    ODR_VERBOSE(logger, "unsupported decoder engine for pdf "
                            << decoder_engine_to_string(with));
    throw UnsupportedDecoderEngine(with);
  }

  if (as == FileType::portable_network_graphics ||
      as == FileType::graphics_interchange_format || as == FileType::jpeg ||
      as == FileType::bitmap_image_file) {
    ODR_VERBOSE(logger, "open as image");
    if (with == DecoderEngine::odr) {
      ODR_VERBOSE(logger, "using odr engine");
      try {
        return std::make_unique<common::ImageFile>(file, as);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as image with odr engine");
      }
      throw NoImageFile();
    }
    ODR_VERBOSE(logger, "unsupported decoder engine for image "
                            << decoder_engine_to_string(with));
    throw UnsupportedDecoderEngine(with);
  }

  if (as == FileType::starview_metafile) {
    ODR_VERBOSE(logger, "open as svm");
    if (with == DecoderEngine::odr) {
      ODR_VERBOSE(logger, "using odr engine");
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        return std::make_unique<svm::SvmFile>(memory_file);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as svm with odr engine");
      }
      throw NoSvmFile();
    }
    ODR_VERBOSE(logger, "unsupported decoder engine for svm "
                            << decoder_engine_to_string(with));
    throw UnsupportedDecoderEngine(with);
  }

  if (as == FileType::text_file) {
    ODR_VERBOSE(logger, "open as text file");
    if (with == DecoderEngine::odr) {
      ODR_VERBOSE(logger, "using odr engine");
      try {
        return std::make_unique<text::TextFile>(file);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as text file with odr engine");
      }
      throw NoTextFile();
    }
    ODR_VERBOSE(logger, "unsupported decoder engine for text file "
                            << decoder_engine_to_string(with));
    throw UnsupportedDecoderEngine(with);
  }

  if (as == FileType::comma_separated_values) {
    ODR_VERBOSE(logger, "open as csv");
    if (with == DecoderEngine::odr) {
      ODR_VERBOSE(logger, "using odr engine");
      try {
        auto text = std::make_shared<text::TextFile>(file);
        return std::make_unique<csv::CsvFile>(text);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as csv with odr engine");
      }
      throw NoCsvFile();
    }
    ODR_VERBOSE(logger, "unsupported decoder engine for csv "
                            << decoder_engine_to_string(with));
    throw UnsupportedDecoderEngine(with);
  }

  if (as == FileType::javascript_object_notation) {
    ODR_VERBOSE(logger, "open as json");
    if (with == DecoderEngine::odr) {
      ODR_VERBOSE(logger, "using odr engine");
      try {
        auto text = std::make_shared<text::TextFile>(file);
        return std::make_unique<json::JsonFile>(text);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as json with odr engine");
      }
      throw NoJsonFile();
    }
    ODR_VERBOSE(logger, "unsupported decoder engine for json "
                            << decoder_engine_to_string(with));
    throw UnsupportedDecoderEngine(with);
  }

  if (as == FileType::zip) {
    ODR_VERBOSE(logger, "open as zip");
    if (with == DecoderEngine::odr) {
      ODR_VERBOSE(logger, "using odr engine");
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        return std::make_unique<zip::ZipFile>(memory_file);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as zip with odr engine");
      }
      throw NoZipFile();
    }
    ODR_VERBOSE(logger, "unsupported decoder engine for zip "
                            << decoder_engine_to_string(with));
    throw UnsupportedDecoderEngine(with);
  }

  if (as == FileType::compound_file_binary_format) {
    ODR_VERBOSE(logger, "open as cfb");
    if (with == DecoderEngine::odr) {
      ODR_VERBOSE(logger, "using odr engine");
      try {
        auto memory_file = std::make_shared<common::MemoryFile>(*file);
        return std::make_unique<cfb::CfbFile>(memory_file);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as cfb with odr engine");
      }
      throw NoCfbFile();
    }
    ODR_VERBOSE(logger, "unsupported decoder engine for cfb "
                            << decoder_engine_to_string(with));
    throw UnsupportedDecoderEngine(with);
  }

  ODR_VERBOSE(logger, "unsupported file type "
                          << file_type_to_string(as) << " with decoder engine "
                          << decoder_engine_to_string(with));
  throw UnsupportedFileType(as);
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(std::shared_ptr<abstract::File> file,
                         const DecodePreference &preference, Logger &logger) {
  std::vector<FileType> probe_types;
  if (preference.as_file_type.has_value()) {
    ODR_VERBOSE(logger, "using preferred file type "
                            << file_type_to_string(*preference.as_file_type));
    probe_types.push_back(*preference.as_file_type);
  } else {
    ODR_VERBOSE(logger, "probe file types");
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
    ODR_VERBOSE(logger, "try opening as file type " << file_type_to_string(as));
    std::vector<DecoderEngine> probe_engines;
    if (preference.with_engine.has_value()) {
      ODR_VERBOSE(logger,
                  "using preferred decoder engine "
                      << decoder_engine_to_string(*preference.with_engine));
      probe_engines.push_back(*preference.with_engine);
    } else {
      ODR_VERBOSE(logger, "probe decoder engines");
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
      ODR_VERBOSE(logger,
                  "with decoder engine " << decoder_engine_to_string(with));
      try {
        return open_file(file, as, with, logger);
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as file type "
                                << file_type_to_string(as)
                                << " with decoder engine "
                                << decoder_engine_to_string(with));
      }
    }
  }

  ODR_VERBOSE(logger, "no suitable file type found");
  throw UnknownFileType();
}

std::unique_ptr<abstract::DocumentFile>
open_strategy::open_document_file(std::shared_ptr<abstract::File> file,
                                  Logger &logger) {
  auto file_type = magic::file_type(*file);
  ODR_VERBOSE(logger,
              "magic determined file type " << file_type_to_string(file_type));

  if (file_type == FileType::zip) {
    ODR_VERBOSE(logger, "open as zip");

    // TODO if `file` is in memory we would copy it unnecessarily
    auto memory_file = std::make_shared<common::MemoryFile>(*file);

    auto zip_file = std::make_unique<zip::ZipFile>(std::move(memory_file));

    auto filesystem = zip_file->archive()->as_filesystem();

    try {
      ODR_VERBOSE(logger, "try open as odf");
      return std::make_unique<odf::OpenDocumentFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as odf");
    }

    try {
      ODR_VERBOSE(logger, "try open as ooxml");
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as ooxml");
    }
  } else if (file_type == FileType::compound_file_binary_format) {
    ODR_VERBOSE(logger, "open as cbf");

    // TODO if `file` is in memory we would copy it unnecessarily
    auto memory_file = std::make_unique<common::MemoryFile>(*file);

    auto cfb_file = std::make_unique<cfb::CfbFile>(std::move(memory_file));

    auto filesystem = cfb_file->archive()->as_filesystem();

    try {
      ODR_VERBOSE(logger, "try open as legacy ms");
      return std::make_unique<oldms::LegacyMicrosoftFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as legacy ms");
    }

    try {
      ODR_VERBOSE(logger, "try open as ooxml");
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as ooxml");
    }
  }

  ODR_VERBOSE(logger, "unsupported file type for document file "
                          << file_type_to_string(file_type));
  throw NoDocumentFile();
}

} // namespace odr::internal
