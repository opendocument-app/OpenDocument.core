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

DiscFile::DiscFile(const char *path) : DiscFile{common::Path(path)} {}

DiscFile::DiscFile(const std::string &path) : DiscFile{common::Path(path)} {}

DiscFile::DiscFile(common::Path path) : m_path{std::move(path)} {
  if (!std::filesystem::is_regular_file(m_path)) {
    throw FileNotFound();
  }
}

FileLocation DiscFile::location() const noexcept { return FileLocation::disc; }

std::size_t DiscFile::size() const {
  return std::filesystem::file_size(m_path.string());
}

common::Path DiscFile::path() const { return m_path; }

std::unique_ptr<std::istream> DiscFile::read() const {
  return std::make_unique<std::ifstream>(m_path.string(),
                                         std::ifstream::binary);
}

TemporaryDiscFile::TemporaryDiscFile(const char *path) : DiscFile{path} {}

TemporaryDiscFile::TemporaryDiscFile(std::string path)
    : DiscFile{std::move(path)} {}

TemporaryDiscFile::TemporaryDiscFile(common::Path path)
    : DiscFile{std::move(path)} {}

TemporaryDiscFile::TemporaryDiscFile(const TemporaryDiscFile &) = default;

TemporaryDiscFile::TemporaryDiscFile(TemporaryDiscFile &&) noexcept = default;

TemporaryDiscFile::~TemporaryDiscFile() {
  std::filesystem::remove(path().string());
}

TemporaryDiscFile &
TemporaryDiscFile::operator=(const TemporaryDiscFile &) = default;

TemporaryDiscFile &
TemporaryDiscFile::operator=(TemporaryDiscFile &&) noexcept = default;

MemoryFile::MemoryFile(std::string data) : m_data{std::move(data)} {}

MemoryFile::MemoryFile(const abstract::File &file) : m_data(file.size(), ' ') {
  auto istream = file.read();
  auto size = (std::int64_t)file.size();
  istream->read(m_data.data(), size);
  if (istream->gcount() != size) {
    throw FileReadError();
  }
}

FileLocation MemoryFile::location() const noexcept {
  return FileLocation::memory;
}

std::size_t MemoryFile::size() const { return m_data.size(); }

const std::string &MemoryFile::content() const { return m_data; }

std::unique_ptr<std::istream> MemoryFile::read() const {
  return std::make_unique<std::istringstream>(m_data);
}

} // namespace odr::internal::common
