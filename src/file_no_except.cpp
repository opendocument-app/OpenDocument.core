#include <glog/logging.h>
#include <odr/file_no_except_test.h>

namespace odr {

std::optional<FileNoExcept>
FileNoExcept::open(const std::string &path) noexcept {
  try {
    return FileNoExcept(File(path));
  } catch (...) {
    LOG(ERROR) << "open failed";
    return {};
  }
}

std::optional<FileNoExcept> FileNoExcept::open(const std::string &path,
                                               const FileType as) noexcept {
  try {
    return FileNoExcept(File(path, as));
  } catch (...) {
    LOG(ERROR) << "openas failed";
    return {};
  }
}

FileType FileNoExcept::type(const std::string &path) noexcept {
  try {
    return File(path).fileType();
  } catch (...) {
    LOG(ERROR) << "type failed";
    return FileType::UNKNOWN;
  }
}

FileMeta FileNoExcept::meta(const std::string &path) noexcept {
  try {
    return File(path).fileMeta();
  } catch (...) {
    LOG(ERROR) << "meta failed";
    return {};
  }
}

FileNoExcept::FileNoExcept(File file) : m_file{std::move(file)} {}

FileType FileNoExcept::fileType() const noexcept {
  try {
    return m_file.fileType();
  } catch (...) {
    LOG(ERROR) << "type failed";
    return FileType::UNKNOWN;
  }
}

FileCategory FileNoExcept::fileCategory() const noexcept {
  try {
    return m_file.fileCategory();
  } catch (...) {
    LOG(ERROR) << "file category failed";
    return FileCategory::UNKNOWN;
  }
}

FileMeta FileNoExcept::fileMeta() const noexcept {
  try {
    return m_file.fileMeta();
  } catch (...) {
    LOG(ERROR) << "meta failed";
    return {};
  }
}

std::optional<DocumentFileNoExcept>
DocumentFileNoExcept::open(const std::string &path) noexcept {
  try {
    return DocumentFileNoExcept(DocumentFile(path));
  } catch (...) {
    LOG(ERROR) << "open failed";
    return {};
  }
}

FileType DocumentFileNoExcept::type(const std::string &path) noexcept {
  try {
    return DocumentFile::type(path);
  } catch (...) {
    LOG(ERROR) << "type failed";
    return FileType::UNKNOWN;
  }
}

FileMeta DocumentFileNoExcept::meta(const std::string &path) noexcept {
  try {
    return DocumentFile::meta(path);
  } catch (...) {
    LOG(ERROR) << "meta failed";
    return {};
  }
}

DocumentFileNoExcept::DocumentFileNoExcept(DocumentFile documentFile)
    : FileNoExcept(documentFile), m_documentFile{std::move(documentFile)} {}

DocumentType DocumentFileNoExcept::documentType() const noexcept {
  try {
    return m_documentFile.documentType();
  } catch (...) {
    LOG(ERROR) << "document type failed";
    return DocumentType::UNKNOWN;
  }
}

} // namespace odr
