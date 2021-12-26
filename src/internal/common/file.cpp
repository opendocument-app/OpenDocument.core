#include <cstdint>
#include <filesystem>
#include <fstream>
#include <internal/abstract/file.h>
#include <internal/common/file.h>
#include <internal/common/path.h>
#include <odr/exceptions.h>
#include <odr/file.h>
#include <sstream>
#include <utility>

namespace odr::internal::common {

DiskFile::DiskFile(const char *path) : DiskFile{common::Path(path)} {}

DiskFile::DiskFile(const std::string &path) : DiskFile{common::Path(path)} {}

DiskFile::DiskFile(common::Path path) : m_path{std::move(path)} {
  if (!std::filesystem::is_regular_file(m_path)) {
    throw FileNotFound();
  }
}

std::size_t DiskFile::size() const {
  return std::filesystem::file_size(m_path.string());
}

std::unique_ptr<std::istream> DiskFile::stream() const {
  return std::make_unique<std::ifstream>(m_path.string(),
                                         std::ifstream::binary);
}

common::Path DiskFile::path() const { return m_path; }

TemporaryDiskFile::TemporaryDiskFile(const char *path) : DiskFile{path} {}

TemporaryDiskFile::TemporaryDiskFile(std::string path)
    : DiskFile{std::move(path)} {}

TemporaryDiskFile::TemporaryDiskFile(common::Path path)
    : DiskFile{std::move(path)} {}

TemporaryDiskFile::TemporaryDiskFile(const TemporaryDiskFile &) = default;

TemporaryDiskFile::TemporaryDiskFile(TemporaryDiskFile &&) noexcept = default;

TemporaryDiskFile::~TemporaryDiskFile() {
  std::filesystem::remove(path().string());
}

TemporaryDiskFile &
TemporaryDiskFile::operator=(const TemporaryDiskFile &) = default;

TemporaryDiskFile &
TemporaryDiskFile::operator=(TemporaryDiskFile &&) noexcept = default;

MemoryFile::MemoryFile(std::string data) : m_data{std::move(data)} {}

MemoryFile::MemoryFile(const abstract::File &file) : m_data(file.size(), ' ') {
  auto istream = file.stream();
  auto size = (std::int64_t)file.size();
  istream->read(m_data.data(), size);
  if (istream->gcount() != size) {
    throw FileReadError();
  }
}

std::size_t MemoryFile::size() const { return m_data.size(); }

std::unique_ptr<std::istream> MemoryFile::stream() const {
  return std::make_unique<std::istringstream>(m_data);
}

const std::string &MemoryFile::content() const { return m_data; }

const char *MemoryFile::data() const { return m_data.data(); }

} // namespace odr::internal::common
