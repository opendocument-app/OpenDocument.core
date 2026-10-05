#include <odr/internal/common/file.hpp>

#include <odr/internal/util/file_util.hpp>

#include <odr/exceptions.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>

namespace odr::internal {

void abstract::File::preserve_source(const AbsPath &path) const {
  const auto source = disk_path();
  if (source.has_value() && std::filesystem::exists(path.path()) &&
      std::filesystem::equivalent(source->path(), path.path())) {
    throw UnsupportedOperation("cannot overwrite a live file resource");
  }
}

DiskFile::DiskFile(const char *path) : DiskFile{AbsPath(path)} {}

DiskFile::DiskFile(const std::string &path) : DiskFile{AbsPath(path)} {}

DiskFile::DiskFile(AbsPath path) : m_path{std::move(path)} {
  if (!std::filesystem::is_regular_file(m_path.path())) {
    throw FileNotFound();
  }
}

FileLocation DiskFile::location() const noexcept { return FileLocation::disk; }

std::size_t DiskFile::size() const {
  return std::filesystem::file_size(m_path.string());
}

std::string DiskFile::name() const { return m_path.basename(); }

std::optional<AbsPath> DiskFile::disk_path() const { return m_path; }

std::optional<std::string_view> DiskFile::memory_data() const {
  return std::nullopt;
}

std::unique_ptr<std::istream> DiskFile::stream() const {
  return std::make_unique<std::ifstream>(util::file::open(m_path.string()));
}

MemoryFile::MemoryFile(std::string data, std::string name)
    : m_data{std::move(data)}, m_name{std::move(name)} {}

MemoryFile::MemoryFile(const File &file)
    : m_data(file.size(), ' '), m_name{file.name()} {
  const auto istream = file.stream();
  const auto size = static_cast<std::streamsize>(m_data.size());
  istream->read(m_data.data(), size);
  if (istream->gcount() != size) {
    throw FileReadError();
  }
}

FileLocation MemoryFile::location() const noexcept {
  return FileLocation::memory;
}

std::size_t MemoryFile::size() const { return m_data.size(); }

std::string MemoryFile::name() const { return m_name; }

std::optional<AbsPath> MemoryFile::disk_path() const { return std::nullopt; }

std::optional<std::string_view> MemoryFile::memory_data() const {
  return m_data;
}

std::unique_ptr<std::istream> MemoryFile::stream() const {
  return std::make_unique<std::istringstream>(m_data);
}

const std::string &MemoryFile::content() const { return m_data; }

} // namespace odr::internal
