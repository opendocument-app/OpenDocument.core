#include <access/CfbStorage.h>
#include <access/Path.h>
#include <access/Storage.h>
#include <access/ZipStorage.h>
#include <common/Document.h>
#include <memory>
#include <odf/OpenDocument.h>
#include <odr/Document.h>
#include <odr/Exception.h>
#include <odr/File.h>
#include <oldms/LegacyMicrosoft.h>
#include <ooxml/OfficeOpenXml.h>
#include <utility>

namespace odr {

namespace {
std::string typeToString(const FileType type) {
  switch (type) {
  case FileType::ZIP:
    return "zip";
  case FileType::COMPOUND_FILE_BINARY_FORMAT:
    return "cfb";
  case FileType::OPENDOCUMENT_TEXT:
    return "odt";
  case FileType::OPENDOCUMENT_PRESENTATION:
    return "odp";
  case FileType::OPENDOCUMENT_SPREADSHEET:
    return "ods";
  case FileType::OPENDOCUMENT_GRAPHICS:
    return "odg";
  case FileType::OFFICE_OPEN_XML_DOCUMENT:
    return "docx";
  case FileType::OFFICE_OPEN_XML_PRESENTATION:
    return "pptx";
  case FileType::OFFICE_OPEN_XML_WORKBOOK:
    return "xlsx";
  case FileType::LEGACY_WORD_DOCUMENT:
    return "doc";
  case FileType::LEGACY_POWERPOINT_PRESENTATION:
    return "ppt";
  case FileType::LEGACY_EXCEL_WORKSHEETS:
    return "xls";
  default:
    return "unnamed";
  }
}

std::unique_ptr<common::File> openImpl(const std::string &path) {
  try {
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::ZipReader>(path);

    try {
      return std::make_unique<odf::OpenDocument>(storage);
    } catch (...) {
      // TODO
    }
    try {
      return std::make_unique<ooxml::OfficeOpenXml>(storage);
    } catch (...) {
      // TODO
    }
  } catch (...) {
    // TODO
  }
  try {
    FileMeta meta;
    std::unique_ptr<access::ReadStorage> storage =
        std::make_unique<access::CfbReader>(path);

    // legacy microsoft
    try {
      return std::make_unique<oldms::LegacyMicrosoft>(storage);
    } catch (...) {
      // TODO
    }

    // encrypted ooxml
    try {
      return std::make_unique<ooxml::OfficeOpenXml>(storage);
    } catch (...) {
      // TODO
    }
  } catch (...) {
    // TODO
  }

  // TODO return unknown file
  throw UnknownFileType();
}

std::unique_ptr<common::Document> openImpl(const std::string &path,
                                           const FileType as) {
  // TODO implement
  throw UnknownFileType();
}
} // namespace

FileType FileMeta::typeByExtension(const std::string &extension) noexcept {
  if (extension == "zip")
    return FileType::ZIP;
  if (extension == "cfb")
    return FileType::COMPOUND_FILE_BINARY_FORMAT;
  if (extension == "odt" || extension == "sxw")
    return FileType::OPENDOCUMENT_TEXT;
  if (extension == "odp" || extension == "sxi")
    return FileType::OPENDOCUMENT_PRESENTATION;
  if (extension == "ods" || extension == "sxc")
    return FileType::OPENDOCUMENT_SPREADSHEET;
  if (extension == "odg" || extension == "sxd")
    return FileType::OPENDOCUMENT_GRAPHICS;
  if (extension == "docx")
    return FileType::OFFICE_OPEN_XML_DOCUMENT;
  if (extension == "pptx")
    return FileType::OFFICE_OPEN_XML_PRESENTATION;
  if (extension == "xlsx")
    return FileType::OFFICE_OPEN_XML_WORKBOOK;
  if (extension == "doc")
    return FileType::LEGACY_WORD_DOCUMENT;
  if (extension == "ppt")
    return FileType::LEGACY_POWERPOINT_PRESENTATION;
  if (extension == "xls")
    return FileType::LEGACY_EXCEL_WORKSHEETS;

  return FileType::UNKNOWN;
}

std::string FileMeta::typeAsString() const noexcept {
  return typeToString(type);
}

FileType File::type(const std::string &path) {
  return File(path).type();
}

FileMeta File::meta(const std::string &path) {
  return File(path).meta();
}

File::File(const std::string &path) : impl_(openImpl(path)) {}

File::File(const std::string &path, FileType as) : impl_(openImpl(path, as)) {}

File::File(File &&file) noexcept {} // TODO

File::~File() = default;

FileType File::type() const noexcept {
  return FileType::UNKNOWN; // TODO
}

const FileMeta & File::meta() const noexcept {
  return impl_->meta();
}

Document File::document() && {
  return std::move(*this);
}

}
