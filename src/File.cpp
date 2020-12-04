#include <access/Path.h>
#include <common/File.h>
#include <odr/File.h>
#include <utility>
#include <OpenStrategy.h>
#include <odr/Exception.h>

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
  return OpenStrategy::types(path);
}

FileType File::type(const std::string &path) {
  return File(path).fileType();
}

FileMeta File::meta(const std::string &path) {
  return File(path).fileMeta();
}

File::File(std::shared_ptr<common::File> file) : m_file{std::move(file)} {
  if (!m_file) throw FileNotFound();
}

File::File(const std::string &path) : File(OpenStrategy::openFile(path)) {}

File::File(const std::string &path, FileType as) : File(OpenStrategy::openFile(path, as)) {}

File::File(const File &) = default;

File::File(File &&file) noexcept = default;

File::~File() = default;

File & File::operator=(const File &) = default;

File & File::operator=(File &&) noexcept = default;

FileType File::fileType() const noexcept {
  return m_file->fileMeta().type;
}

FileCategory File::fileCategory() const noexcept {
  return FileMeta::categoryByType(fileType());
}

FileMeta File::fileMeta() const noexcept {
  return m_file->fileMeta();
}

FileType DocumentFile::type(const std::string &path) {
  return DocumentFile(path).fileType();
}

FileMeta DocumentFile::meta(const std::string &path) {
  return DocumentFile(path).fileMeta();
}

DocumentFile::DocumentFile(const std::string &path) : File(OpenStrategy::openDocumentFile(path)) {}

DocumentFile::DocumentFile(const File &file) : File(file) {
  // TODO throw if not document file
}

DocumentFile::DocumentFile(File &&file) : File(std::move(file)) {
  // TODO throw if not document file
}

bool DocumentFile::encrypted() const {
  return true; // TODO
}

EncryptionState DocumentFile::encryptionState() const {
  return impl().encryptionState();
}

bool DocumentFile::decrypt(const std::string &password) {
  return impl().decrypt(password);
}

DocumentType DocumentFile::documentType() const {
  return impl().documentType();
}

DocumentMeta DocumentFile::documentMeta() const {
  return impl().documentMeta();
}

Document DocumentFile::document() const {
  return Document(impl().document());
}

common::DocumentFile & DocumentFile::impl() const {
  return static_cast<common::DocumentFile &>(*m_file);
}

}
