#include <odr/internal/common/temporary_file.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/common/random.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <cstdint>
#include <fstream>
#include <utility>

namespace odr::internal {

namespace {

void remove_quietly(const AbsPath &path) noexcept {
  try {
    std::error_code error_code;
    std::filesystem::remove(path.string(), error_code);
  } catch (...) {
    return; // Path conversion may allocate; cleanup must not throw.
  }
}

} // namespace

TemporaryDiskFile::TemporaryDiskFile(const char *path) : DiskFile{path} {}

TemporaryDiskFile::TemporaryDiskFile(const std::string &path)
    : DiskFile{path} {}

TemporaryDiskFile::TemporaryDiskFile(AbsPath path)
    : DiskFile{std::move(path)} {}

TemporaryDiskFile::TemporaryDiskFile(TemporaryDiskFile &&other) noexcept
    : DiskFile{std::move(static_cast<DiskFile &>(other))},
      m_owns_path{std::exchange(other.m_owns_path, false)} {}

TemporaryDiskFile::~TemporaryDiskFile() {
  if (!m_owns_path) {
    return;
  }
  remove_quietly(path());
}

TemporaryDiskFile &
TemporaryDiskFile::operator=(TemporaryDiskFile &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (m_owns_path) {
    remove_quietly(path());
  }
  DiskFile::operator=(std::move(static_cast<DiskFile &>(other)));
  m_owns_path = std::exchange(other.m_owns_path, false);
  return *this;
}

const TemporaryDiskFileFactory &TemporaryDiskFileFactory::system_default() {
  static TemporaryDiskFileFactory instance(
      AbsPath(std::filesystem::temp_directory_path()),
      default_random_file_name_generator());
  return instance;
}

TemporaryDiskFileFactory::RandomFileNameGenerator
TemporaryDiskFileFactory::default_random_file_name_generator() {
  return [] { return random_string(10); };
}

TemporaryDiskFileFactory::TemporaryDiskFileFactory(
    AbsPath directory, RandomFileNameGenerator random_file_name_generator)
    : m_directory{std::move(directory)},
      m_random_file_name_generator{std::move(random_file_name_generator)} {}

TemporaryDiskFile
TemporaryDiskFileFactory::copy(const abstract::File &file) const {
  return copy(*file.stream());
}

TemporaryDiskFile TemporaryDiskFileFactory::copy(std::istream &in) const {
  return create([&](std::ostream &out) { util::stream::pipe(in, out); });
}

TemporaryDiskFile TemporaryDiskFileFactory::create(
    const std::function<void(std::ostream &)> &write) const {
  std::ofstream file;
  AbsPath file_path;
  for (std::uint32_t attempt = 0; attempt < 128; ++attempt) {
    file_path = m_directory.join(RelPath(m_random_file_name_generator()));
    file.open(file_path.string(), std::ios::binary | std::ios::noreplace);
    if (file.is_open()) {
      break;
    }
    std::error_code error;
    if (!std::filesystem::exists(file_path.string(), error) || error) {
      throw FileWriteError(file_path.string());
    }
    file.clear();
  }
  if (!file.is_open()) {
    throw FileWriteError(file_path.string());
  }

  try {
    std::filesystem::permissions(file_path.path(),
                                 std::filesystem::perms::owner_read |
                                     std::filesystem::perms::owner_write);
    write(file);
    file.close();
    if (!file) {
      throw FileWriteError(file_path.string());
    }
  } catch (...) {
    file.exceptions(std::ios::goodbit);
    file.close();
    remove_quietly(file_path);
    throw;
  }

  return TemporaryDiskFile(file_path);
}

} // namespace odr::internal
