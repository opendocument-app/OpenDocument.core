#include <abstract/file.h>
#include <common/file.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <open_strategy.h>
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

FileCategory FileMeta::categoryByType(const FileType type) noexcept {
  switch (type) {
  case FileType::ZIP:
  case FileType::COMPOUND_FILE_BINARY_FORMAT:
    return FileCategory::ARCHIVE;
  case FileType::OPENDOCUMENT_TEXT:
  case FileType::OPENDOCUMENT_PRESENTATION:
  case FileType::OPENDOCUMENT_SPREADSHEET:
  case FileType::OPENDOCUMENT_GRAPHICS:
  case FileType::OFFICE_OPEN_XML_DOCUMENT:
  case FileType::OFFICE_OPEN_XML_PRESENTATION:
  case FileType::OFFICE_OPEN_XML_WORKBOOK:
  case FileType::LEGACY_WORD_DOCUMENT:
  case FileType::LEGACY_POWERPOINT_PRESENTATION:
  case FileType::LEGACY_EXCEL_WORKSHEETS:
    return FileCategory::DOCUMENT;
  default:
    return FileCategory::UNKNOWN;
  }
}

std::string FileMeta::typeAsString() const noexcept {
  return typeToString(type);
}

std::vector<FileType> File::types(const std::string &path) {
  return open_strategy::types(std::make_shared<common::DiscFile>(path));
}

FileType File::type(const std::string &path) { return File(path).fileType(); }

FileMeta File::meta(const std::string &path) { return File(path).fileMeta(); }

File::File(std::shared_ptr<abstract::DecodedFile> impl)
    : m_impl{std::move(impl)} {
  if (!m_impl)
    throw FileNotFound();
}

File::File(const std::string &path)
    : File(open_strategy::open_file(std::make_shared<common::DiscFile>(path))) {
}

File::File(const std::string &path, FileType as)
    : File(open_strategy::open_file(std::make_shared<common::DiscFile>(path),
                                    as)) {}

FileType File::fileType() const noexcept { return m_impl->file_meta().type; }

FileCategory File::fileCategory() const noexcept {
  return FileMeta::categoryByType(fileType());
}

FileMeta File::fileMeta() const noexcept { return m_impl->file_meta(); }

ImageFile File::imageFile() const {
  auto imageFile = std::dynamic_pointer_cast<abstract::ImageFile>(m_impl);
  if (!imageFile)
    throw NoImageFile();
  return ImageFile(imageFile);
}

DocumentFile File::documentFile() const {
  auto documentFile = std::dynamic_pointer_cast<abstract::DocumentFile>(m_impl);
  if (!documentFile)
    throw NoDocumentFile();
  return DocumentFile(documentFile);
}

ImageFile::ImageFile(std::shared_ptr<abstract::ImageFile> impl)
    : File(impl), m_impl{std::move(impl)} {}

FileType DocumentFile::type(const std::string &path) {
  return DocumentFile(path).fileType();
}

FileMeta DocumentFile::meta(const std::string &path) {
  return DocumentFile(path).fileMeta();
}

DocumentFile::DocumentFile(std::shared_ptr<abstract::DocumentFile> impl)
    : File(impl), m_impl{std::move(impl)} {}

DocumentFile::DocumentFile(const std::string &path)
    : DocumentFile(open_strategy::open_document_file(
          std::make_shared<common::DiscFile>(path))) {}

bool DocumentFile::password_encrypted() const {
  return m_impl->password_encrypted();
}

EncryptionState DocumentFile::encryption_state() const {
  return m_impl->encryption_state();
}

bool DocumentFile::decrypt(const std::string &password) {
  return m_impl->decrypt(password);
}

DocumentType DocumentFile::document_type() const {
  return m_impl->document_type();
}

DocumentMeta DocumentFile::document_meta() const {
  return m_impl->document_meta();
}

Document DocumentFile::document() const { return Document(m_impl->document()); }

} // namespace odr
