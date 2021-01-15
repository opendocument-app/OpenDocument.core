#include <common/file.h>
#include <filesystem>
#include <fstream>
#include <odr/file.h>
#include <sstream>

namespace odr::common {

DiscFile::DiscFile(const char *path) : m_path{path} {}

DiscFile::DiscFile(std::string path) : m_path{std::move(path)} {}

DiscFile::DiscFile(common::Path path) : m_path{std::move(path)} {}

FileType DiscFile::file_type() const noexcept { return FileType::UNKNOWN; }

FileMeta DiscFile::file_meta() const noexcept { return {}; }

FileLocation DiscFile::file_location() const noexcept {
  return FileLocation::DISC;
}

std::size_t DiscFile::size() const {
  return std::filesystem::file_size(m_path.string());
}

common::Path DiscFile::path() const { return m_path; }

std::unique_ptr<std::istream> DiscFile::data() const {
  return std::make_unique<std::ifstream>(m_path, std::ifstream::binary);
}

TemporaryDiscFile::TemporaryDiscFile(const char *path) : DiscFile{path} {}

TemporaryDiscFile::TemporaryDiscFile(std::string path)
    : DiscFile{std::move(path)} {}

TemporaryDiscFile::TemporaryDiscFile(common::Path path)
    : DiscFile{std::move(path)} {}

TemporaryDiscFile::~TemporaryDiscFile() {
  std::filesystem::remove(path().string());
}

MemoryFile::MemoryFile(std::string data) : m_data{std::move(data)} {}

FileType MemoryFile::file_type() const noexcept { return FileType::UNKNOWN; }

FileMeta MemoryFile::file_meta() const noexcept { return {}; }

FileLocation MemoryFile::file_location() const noexcept {
  return FileLocation::MEMORY;
}

std::size_t MemoryFile::size() const { return m_data.size(); }

const std::string &MemoryFile::content() const { return m_data; }

std::unique_ptr<std::istream> MemoryFile::data() const {
  return std::make_unique<std::istringstream>(m_data);
}

} // namespace odr::common
