#include <odr/internal/common/file.hpp>

#include <odr/internal/util/file_util.hpp>

#include <odr/exceptions.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace odr::internal {

DiskFile::DiskFile(const char *path) : DiskFile{Path(path)} {}

DiskFile::DiskFile(const std::string &path) : DiskFile{Path(path)} {}

DiskFile::DiskFile(Path path) {
  if (!std::filesystem::is_regular_file(path.path())) {
    throw FileNotFound();
  }
  m_path = AbsPath(std::filesystem::absolute(path.path()));
}

FileLocation DiskFile::location() const noexcept { return FileLocation::disk; }

std::size_t DiskFile::size() const {
  return std::filesystem::file_size(m_path.string());
}

std::optional<AbsPath> DiskFile::disk_path() const { return m_path; }

const char *DiskFile::memory_data() const { return nullptr; }

std::unique_ptr<std::istream> DiskFile::stream() const {
  return std::make_unique<std::ifstream>(util::file::open(m_path.string()));
}

MemoryFile::MemoryFile(std::string data) : m_data{std::move(data)} {}

MemoryFile::MemoryFile(const abstract::File &file) : m_data(file.size(), ' ') {
  auto istream = file.stream();
  auto size = (std::int64_t)file.size();
  istream->read(m_data.data(), size);
  if (istream->gcount() != size) {
    throw FileReadError();
  }
}

FileLocation MemoryFile::location() const noexcept {
  return FileLocation::disk;
}

std::size_t MemoryFile::size() const { return m_data.size(); }

std::optional<AbsPath> MemoryFile::disk_path() const { return std::nullopt; }

const char *MemoryFile::memory_data() const { return m_data.data(); }

std::unique_ptr<std::istream> MemoryFile::stream() const {
  return std::make_unique<std::istringstream>(m_data);
}

const std::string &MemoryFile::content() const { return m_data; }

} // namespace odr::internal
