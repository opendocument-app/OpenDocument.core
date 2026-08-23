#include <odr/internal/open_strategy.hpp>

#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/cfb/cfb_file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/image_file.hpp>
#include <odr/internal/common/media_file.hpp>
#include <odr/internal/csv/csv_file.hpp>
#include <odr/internal/font/font_file.hpp>
#include <odr/internal/json/json_file.hpp>
#include <odr/internal/magic.hpp>
#include <odr/internal/markdown/markdown_file.hpp>
#include <odr/internal/odf/odf_file.hpp>
#include <odr/internal/oldms/oldms_file.hpp>
#include <odr/internal/ooxml/ooxml_file.hpp>
#include <odr/internal/pdf/pdf_file.hpp>
#include <odr/internal/svg/svg_file.hpp>
#include <odr/internal/svm/svm_file.hpp>
#include <odr/internal/xml/xml_file.hpp>
#include <odr/internal/zip/zip_file.hpp>

#include <algorithm>
#include <memory>

namespace odr::internal {

namespace {

/// Orders by position in @p priority; anything not listed sorts last.
template <typename T> auto priority_comparator(const std::vector<T> &priority) {
  return [&priority](const T &a, const T &b) {
    const auto a_it = std::ranges::find(priority, a);
    const auto b_it = std::ranges::find(priority, b);

    if (b_it == std::ranges::end(priority)) {
      return a_it != std::ranges::end(priority);
    }
    if (a_it == std::ranges::end(priority)) {
      return false;
    }
    return a_it < b_it;
  };
}

/// Decodes @p file as exactly @p as, or throws the format's "not a ..."
/// exception (@ref UnsupportedFileType for a type we cannot decode at all).
std::unique_ptr<abstract::DecodedFile>
open_file_as(const std::shared_ptr<abstract::File> &file, const FileType as,
             const Logger &logger) {
  if (as == FileType::opendocument_text ||
      as == FileType::opendocument_presentation ||
      as == FileType::opendocument_spreadsheet ||
      as == FileType::opendocument_graphics) {
    ODR_VERBOSE(logger, "open as odf");
    try {
      auto zip_file = std::make_unique<zip::ZipFile>(file);
      auto filesystem = zip_file->archive()->as_filesystem();
      return std::make_unique<odf::OpenDocumentFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as odf");
    }
    throw NoOpenDocumentFile();
  }

  if (as == FileType::office_open_xml_document ||
      as == FileType::office_open_xml_presentation ||
      as == FileType::office_open_xml_workbook ||
      as == FileType::office_open_xml_encrypted) {
    ODR_VERBOSE(logger, "open as ooxml");
    try {
      auto zip_file = std::make_unique<zip::ZipFile>(file);
      auto filesystem = zip_file->archive()->as_filesystem();
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as ooxml zip");
    }
    try {
      auto cfb_file = std::make_unique<cfb::CfbFile>(file);
      auto filesystem = cfb_file->archive()->as_filesystem();
      return std::make_unique<ooxml::OfficeOpenXmlFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as ooxml cfb");
    }
    throw NoOfficeOpenXmlFile();
  }

  if (as == FileType::legacy_word_document ||
      as == FileType::legacy_powerpoint_presentation ||
      as == FileType::legacy_excel_worksheets) {
    ODR_VERBOSE(logger, "open as legacy ms");
    try {
      auto cfb_file = std::make_unique<cfb::CfbFile>(file);
      auto filesystem = cfb_file->archive()->as_filesystem();
      return std::make_unique<oldms::LegacyMicrosoftFile>(filesystem);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as legacy ms");
    }
    throw NoLegacyMicrosoftFile();
  }

  if (as == FileType::portable_document_format) {
    ODR_VERBOSE(logger, "open as pdf");
    try {
      return std::make_unique<pdf::PdfFile>(file);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as pdf");
    }
    throw NoPdfFile();
  }

  if (as == FileType::starview_metafile) {
    ODR_VERBOSE(logger, "open as svm");
    try {
      return std::make_unique<svm::SvmFile>(file);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as svm");
    }
    throw NoSvmFile();
  }

  if (as == FileType::scalable_vector_graphics) {
    ODR_VERBOSE(logger, "open as svg");
    try {
      auto text = std::make_shared<text::TextFile>(file);
      return std::make_unique<svg::SvgFile>(
          std::make_shared<xml::XmlFile>(text));
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as svg");
    }
    throw NoSvgFile();
  }

  // no decoder below: the bytes go to the browser as they are, so only the
  // category has to be right
  const FileCategory category = file_category_by_file_type(as);
  if (category == FileCategory::image) {
    ODR_VERBOSE(logger, "open as image");
    return std::make_unique<ImageFile>(file, as);
  }
  if (category == FileCategory::audio || category == FileCategory::video) {
    ODR_VERBOSE(logger, "open as media");
    return std::make_unique<MediaFile>(file, as);
  }

  if (as == FileType::truetype_font || as == FileType::opentype_font) {
    ODR_VERBOSE(logger, "open as font");
    try {
      return std::make_unique<font::FontFile>(file, as);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as font");
    }
    throw NoFontFile();
  }

  if (as == FileType::text_file) {
    ODR_VERBOSE(logger, "open as text file");
    try {
      return std::make_unique<text::TextFile>(file);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as text file");
    }
    throw NoTextFile();
  }

  if (as == FileType::comma_separated_values) {
    ODR_VERBOSE(logger, "open as csv");
    try {
      auto text = std::make_shared<text::TextFile>(file);
      return std::make_unique<csv::CsvFile>(text);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as csv");
    }
    throw NoCsvFile();
  }

  if (as == FileType::javascript_object_notation) {
    ODR_VERBOSE(logger, "open as json");
    try {
      auto text = std::make_shared<text::TextFile>(file);
      return std::make_unique<json::JsonFile>(text);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as json");
    }
    throw NoJsonFile();
  }

  if (as == FileType::markdown) {
    ODR_VERBOSE(logger, "open as markdown");
    // Nothing rejects: md4c is total, so there is no detection to fail here.
    auto text = std::make_shared<text::TextFile>(file);
    return std::make_unique<markdown::MarkdownFile>(std::move(text));
  }

  if (as == FileType::xml) {
    ODR_VERBOSE(logger, "open as xml");
    try {
      auto text = std::make_shared<text::TextFile>(file);
      return std::make_unique<xml::XmlFile>(text);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as xml");
    }
    throw NoXmlFile();
  }

  if (as == FileType::zip) {
    ODR_VERBOSE(logger, "open as zip");
    try {
      return std::make_unique<zip::ZipFile>(file);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as zip");
    }
    throw NoZipFile();
  }

  if (as == FileType::compound_file_binary_format) {
    ODR_VERBOSE(logger, "open as cfb");
    try {
      return std::make_unique<cfb::CfbFile>(file);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as cfb");
    }
    throw NoCfbFile();
  }

  ODR_ERROR(logger, "unsupported file type " << file_type_to_string(as));
  throw UnsupportedFileType(as);
}

} // namespace

std::vector<FileType>
open_strategy::list_file_types(const std::shared_ptr<abstract::File> &file,
                               const Logger &logger) {
  std::vector<FileType> result;

  auto file_type = magic::file_type(*file);
  ODR_VERBOSE(logger,
              "magic determined file type " << file_type_to_string(file_type));

  if (file_type == FileType::zip) {
    ODR_VERBOSE(logger, "open as zip");

    // a container the magic promised but that does not open is just another
    // failed probe here — the callers degrade on an empty result
    try {
      zip::ZipFile zip_file(file);
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
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as zip");
    }
  } else if (file_type == FileType::compound_file_binary_format) {
    ODR_VERBOSE(logger, "open as cbf");

    try {
      cfb::CfbFile cfb_file(file);
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
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as cfb");
    }
  } else if (file_type == FileType::starview_metafile) {
    try {
      ODR_VERBOSE(logger, "try open as svm");
      result.push_back(svm::SvmFile(file).file_type());
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

      // an svg has no signature; only the xml root element tells it from plain
      // xml, so both are reported
      try {
        ODR_VERBOSE(logger, "try open as xml");
        auto xml_file = std::make_shared<xml::XmlFile>(text);
        result.push_back(xml_file->file_type());

        if (svg::is_svg_file(*xml_file)) {
          ODR_VERBOSE(logger, "open as svg");
          result.push_back(svg::SvgFile(xml_file).file_type());
        }
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as xml");
      }
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as text");
    }
  } else {
    ODR_VERBOSE(logger, "anything else");
    result.push_back(file_type);
  }

  return result;
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(const std::shared_ptr<abstract::File> &file,
                         const Logger &logger) {
  auto file_type = magic::file_type(*file);
  ODR_VERBOSE(logger,
              "magic determined file type " << file_type_to_string(file_type));

  if (file_type == FileType::zip) {
    ODR_VERBOSE(logger, "open as zip");

    auto zip_file = std::make_unique<zip::ZipFile>(file);

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
  }
  if (file_type == FileType::compound_file_binary_format) {
    ODR_VERBOSE(logger, "open as cbf");

    auto cfb_file = std::make_unique<cfb::CfbFile>(file);

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
  }
  if (file_type == FileType::portable_document_format) {
    ODR_VERBOSE(logger, "open as pdf");
    return std::make_unique<pdf::PdfFile>(file);
  }
  if (file_type == FileType::starview_metafile) {
    ODR_VERBOSE(logger, "open as svm");
    return std::make_unique<svm::SvmFile>(file);
  }
  // see `open_file_as` — no decoder, only the category has to be right
  const FileCategory file_category = file_category_by_file_type(file_type);
  if (file_category == FileCategory::image) {
    ODR_VERBOSE(logger, "open as image");
    return std::make_unique<ImageFile>(file, file_type);
  }
  if (file_category == FileCategory::audio ||
      file_category == FileCategory::video) {
    ODR_VERBOSE(logger, "open as media");
    return std::make_unique<MediaFile>(file, file_type);
  }
  if (file_type == FileType::truetype_font ||
      file_type == FileType::opentype_font) {
    ODR_VERBOSE(logger, "open as font");
    return std::make_unique<font::FontFile>(file, file_type);
  }
  if (file_type == FileType::unknown) {
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

      // svg is read off the parse xml already did: it is the more specific
      // reading of the same bytes, and xml is the last resort before the line
      // list
      try {
        ODR_VERBOSE(logger, "try open as xml");
        auto xml_file = std::make_unique<xml::XmlFile>(text);

        if (!svg::is_svg_file(*xml_file)) {
          ODR_VERBOSE(logger, "not an svg");
          // handed on as it is, so the parse is not repeated
          return xml_file;
        }

        ODR_VERBOSE(logger, "open as svg");
        return std::make_unique<svg::SvgFile>(
            std::shared_ptr<xml::XmlFile>(std::move(xml_file)));
      } catch (...) {
        ODR_VERBOSE(logger, "failed to open as xml");
      }

      ODR_VERBOSE(logger, "open as text file");
      // TODO looks dirty
      return std::make_unique<text::TextFile>(file);
    } catch (...) {
      ODR_VERBOSE(logger, "failed to open as text");
    }

    ODR_ERROR(logger, "unknown file type");
    throw UnknownFileType();
  }

  ODR_ERROR(logger, "unsupported file type");
  throw UnsupportedFileType(file_type);
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(const std::shared_ptr<abstract::File> &file,
                         FileType as, const Logger &logger) {
  DecodePreference preference;
  preference.as_file_type = as;
  return open_file(file, preference, logger);
}

std::unique_ptr<abstract::DecodedFile>
open_strategy::open_file(const std::shared_ptr<abstract::File> &file,
                         const DecodePreference &preference,
                         const Logger &logger) {
  std::vector<FileType> probe_types;
  if (preference.as_file_type.has_value()) {
    ODR_VERBOSE(logger, "using preferred file type "
                            << file_type_to_string(*preference.as_file_type));
    probe_types.push_back(*preference.as_file_type);
  } else {
    ODR_VERBOSE(logger, "probe file types");
    std::vector<FileType> detected_types = list_file_types(file, logger);
    probe_types.insert(probe_types.end(), detected_types.begin(),
                       detected_types.end());
    auto probe_types_end = std::ranges::unique(probe_types).begin();
    probe_types.erase(probe_types_end, probe_types.end());
    // more specific file types are further down the list, so we bring them up
    std::ranges::reverse(probe_types);
  }

  std::ranges::stable_sort(probe_types,
                           priority_comparator(preference.file_type_priority));

  for (FileType as : probe_types) {
    ODR_VERBOSE(logger, "try opening as file type " << file_type_to_string(as));
    try {
      return open_file_as(file, as, logger);
    } catch (...) {
      ODR_VERBOSE(logger,
                  "failed to open as file type " << file_type_to_string(as));
    }
  }

  ODR_ERROR(logger, "no suitable file type found");
  throw UnknownFileType();
}

std::unique_ptr<abstract::DocumentFile>
open_strategy::open_document_file(const std::shared_ptr<abstract::File> &file,
                                  const Logger &logger) {
  auto file_type = magic::file_type(*file);
  ODR_VERBOSE(logger,
              "magic determined file type " << file_type_to_string(file_type));

  if (file_type == FileType::zip) {
    ODR_VERBOSE(logger, "open as zip");

    auto zip_file = std::make_unique<zip::ZipFile>(file);

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

    auto cfb_file = std::make_unique<cfb::CfbFile>(file);

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

  ODR_ERROR(logger, "unsupported file type for document file "
                        << file_type_to_string(file_type));
  throw NoDocumentFile();
}

} // namespace odr::internal
