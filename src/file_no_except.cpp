#include <glog/logging.h>
#include <odr/document.h>
#include <odr/file_no_except.h>

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

FileNoExcept::FileNoExcept(File file) : m_file{std::move(file)} {}

FileLocation FileNoExcept::location() const noexcept {
  return m_file.location();
}

std::size_t FileNoExcept::size() const { return m_file.size(); }

std::unique_ptr<std::istream> FileNoExcept::read() const {
  return m_file.read();
}

std::optional<DecodedFileNoExcept>
DecodedFileNoExcept::open(const std::string &path) noexcept {
  try {
    return DecodedFileNoExcept(DecodedFile(path));
  } catch (...) {
    LOG(ERROR) << "open failed";
    return {};
  }
}

std::optional<DecodedFileNoExcept>
DecodedFileNoExcept::open(const std::string &path, const FileType as) noexcept {
  try {
    return DecodedFileNoExcept(DecodedFile(path, as));
  } catch (...) {
    LOG(ERROR) << "openas failed";
    return {};
  }
}

FileType DecodedFileNoExcept::type(const std::string &path) noexcept {
  try {
    return DecodedFile(path).file_type();
  } catch (...) {
    LOG(ERROR) << "type failed";
    return FileType::UNKNOWN;
  }
}

FileMeta DecodedFileNoExcept::meta(const std::string &path) noexcept {
  try {
    return DecodedFile(path).file_meta();
  } catch (...) {
    LOG(ERROR) << "meta failed";
    return {};
  }
}

DecodedFileNoExcept::DecodedFileNoExcept(DecodedFile file)
    : m_file{std::move(file)} {}

FileType DecodedFileNoExcept::file_type() const noexcept {
  try {
    return m_file.file_type();
  } catch (...) {
    LOG(ERROR) << "type failed";
    return FileType::UNKNOWN;
  }
}

FileCategory DecodedFileNoExcept::file_category() const noexcept {
  try {
    return m_file.file_category();
  } catch (...) {
    LOG(ERROR) << "file category failed";
    return FileCategory::UNKNOWN;
  }
}

FileMeta DecodedFileNoExcept::file_meta() const noexcept {
  try {
    return m_file.file_meta();
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

DocumentFileNoExcept::DocumentFileNoExcept(DocumentFile document_file)
    : DecodedFileNoExcept(document_file), m_document_file{
                                              std::move(document_file)} {}

DocumentType DocumentFileNoExcept::document_type() const noexcept {
  try {
    return m_document_file.document_type();
  } catch (...) {
    LOG(ERROR) << "document type failed";
    return DocumentType::UNKNOWN;
  }
}

} // namespace odr
