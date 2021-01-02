#include <common/document.h>
#include <common/file.h>
#include <filesystem>
#include <fstream>
#include <odr/document.h>
#include <odr/file.h>
#include <sstream>

namespace odr::common {

FileCategory File::fileCategory() const noexcept {
  return FileMeta::categoryByType(fileType());
}

DiscFile::DiscFile(const char *path) : m_path{path} {}

DiscFile::DiscFile(std::string path) : m_path{std::move(path)} {}

DiscFile::DiscFile(common::Path path) : m_path{std::move(path)} {}

FileType DiscFile::fileType() const noexcept { return FileType::UNKNOWN; }

FileMeta DiscFile::fileMeta() const noexcept { return {}; }

FileLocation DiscFile::fileLocation() const noexcept {
  return FileLocation::DISC;
}

std::size_t DiscFile::size() const {
  return std::filesystem::file_size(m_path.string());
}

common::Path DiscFile::path() const { return m_path; }

std::unique_ptr<std::istream> DiscFile::data() const {
  return std::make_unique<std::ifstream>(m_path, std::ifstream::binary);
}

MemoryFile::MemoryFile(std::string data) : m_data{std::move(data)} {}

FileType MemoryFile::fileType() const noexcept { return FileType::UNKNOWN; }

FileMeta MemoryFile::fileMeta() const noexcept { return {}; }

FileLocation MemoryFile::fileLocation() const noexcept {
  return FileLocation::MEMORY;
}

std::size_t MemoryFile::size() const { return m_data.size(); }

const std::string &MemoryFile::content() const { return m_data; }

std::unique_ptr<std::istream> MemoryFile::data() const {
  return std::make_unique<std::istringstream>(m_data);
}

FileCategory TextFile::fileCategory() const noexcept {
  return FileCategory::TEXT;
}

FileCategory ImageFile::fileCategory() const noexcept {
  return FileCategory::IMAGE;
}

FileCategory ArchiveFile::fileCategory() const noexcept {
  return FileCategory::ARCHIVE;
}

DocumentType DocumentFile::documentType() const {
  return document()->documentType();
}

DocumentMeta DocumentFile::documentMeta() const {
  return document()->documentMeta();
}

} // namespace odr::common
