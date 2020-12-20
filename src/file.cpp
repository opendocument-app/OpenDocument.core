#include <access/path.h>
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
  return OpenStrategy::types(path);
}

FileType File::type(const std::string &path) { return File(path).fileType(); }

FileMeta File::meta(const std::string &path) { return File(path).fileMeta(); }

File::File(std::shared_ptr<common::File> impl) : m_impl{std::move(impl)} {
  if (!m_impl)
    throw FileNotFound();
}

File::File(const std::string &path) : File(OpenStrategy::openFile(path)) {}

File::File(const std::string &path, FileType as)
    : File(OpenStrategy::openFile(path, as)) {}

FileType File::fileType() const noexcept { return m_impl->fileMeta().type; }

FileCategory File::fileCategory() const noexcept {
  return FileMeta::categoryByType(fileType());
}

FileMeta File::fileMeta() const noexcept { return m_impl->fileMeta(); }

std::unique_ptr<std::istream> File::data() const { return m_impl->data(); }

ImageFile File::imageFile() const {
  auto imageFile = std::dynamic_pointer_cast<common::ImageFile>(m_impl);
  if (!imageFile)
    throw NoImageFile();
  return ImageFile(imageFile);
}

DocumentFile File::documentFile() const {
  auto documentFile = std::dynamic_pointer_cast<common::DocumentFile>(m_impl);
  if (!documentFile)
    throw NoDocumentFile();
  return DocumentFile(documentFile);
}

ImageFile::ImageFile(std::shared_ptr<common::ImageFile> impl)
    : File(impl), m_impl{std::move(impl)} {}

FileType DocumentFile::type(const std::string &path) {
  return DocumentFile(path).fileType();
}

FileMeta DocumentFile::meta(const std::string &path) {
  return DocumentFile(path).fileMeta();
}

DocumentFile::DocumentFile(std::shared_ptr<common::DocumentFile> impl)
    : File(impl), m_impl{std::move(impl)} {}

DocumentFile::DocumentFile(const std::string &path)
    : DocumentFile(OpenStrategy::openDocumentFile(path)) {}

bool DocumentFile::passwordEncrypted() const {
  return m_impl->passwordEncrypted();
}

EncryptionState DocumentFile::encryptionState() const {
  return m_impl->encryptionState();
}

bool DocumentFile::decrypt(const std::string &password) {
  return m_impl->decrypt(password);
}

DocumentType DocumentFile::documentType() const {
  return m_impl->documentType();
}

DocumentMeta DocumentFile::documentMeta() const {
  return m_impl->documentMeta();
}

Document DocumentFile::document() const { return Document(m_impl->document()); }

} // namespace odr
