#include <odr/FileNoExcept.h>
#include <odr/DocumentNoExcept.h>
#include <glog/logging.h>

namespace odr {

FileNoExcept::FileNoExcept(File &&file) : m_impl{std::make_unique<File>(std::move(file))} {}

FileNoExcept::FileNoExcept(std::unique_ptr<File> file) : m_impl{std::move(file)} {}

FileType FileNoExcept::fileType() const noexcept {
  try {
    return m_impl->fileType();
  } catch (...) {
    LOG(ERROR) << "type failed";
    return FileType::UNKNOWN;
  }
}

FileCategory FileNoExcept::fileCategory() const noexcept {
  try {
    return m_impl->fileCategory();
  } catch (...) {
    LOG(ERROR) << "file category failed";
    return FileCategory::UNKNOWN;
  }
}

const FileMeta &FileNoExcept::fileMeta() const noexcept {
  try {
    return m_impl->fileMeta();
  } catch (...) {
    LOG(ERROR) << "meta failed";
    return {};
  }
}

DocumentNoExcept FileNoExcept::document() && noexcept {
  return DocumentNoExcept(std::move(*this));
}

}
