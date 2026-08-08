#include <odr/internal/common/temporary_file.hpp>

#include <odr/internal/common/random.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <cassert>
#include <fstream>
#include <utility>

namespace odr::internal {

namespace {

void remove_quietly(const AbsPath &path) {
  std::error_code error_code;
  std::filesystem::remove(path.string(), error_code);
}

} // namespace

TemporaryDiskFile::TemporaryDiskFile(const char *path) : DiskFile{path} {}

TemporaryDiskFile::TemporaryDiskFile(const std::string &path)
    : DiskFile{path} {}

TemporaryDiskFile::TemporaryDiskFile(AbsPath path)
    : DiskFile{std::move(path)} {}

TemporaryDiskFile::TemporaryDiskFile(TemporaryDiskFile &&other) noexcept
    : DiskFile{std::move(other)},
      m_owns_path{std::exchange(other.m_owns_path, false)} {}

TemporaryDiskFile::~TemporaryDiskFile() {
  if (!m_owns_path) {
    return;
  }
  assert(disk_path().has_value());
  remove_quietly(*disk_path());
}

TemporaryDiskFile &
TemporaryDiskFile::operator=(TemporaryDiskFile &&other) noexcept {
  if (this == &other) {
    return *this;
  }
  if (m_owns_path) {
    assert(disk_path().has_value());
    remove_quietly(*disk_path());
  }
  DiskFile::operator=(std::move(other));
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
  std::fstream file;
  AbsPath file_path;

  while (true) {
    std::string file_name = m_random_file_name_generator();
    file_path = m_directory.join(RelPath(file_name));

    file.open(file_path.string(), std::ios_base::in | std::ios_base::out);

    if (!file.is_open()) {
      file.clear();
      file.open(file_path.string(), std::ios_base::out);
      break;
    }

    file.close();
  }

  util::stream::pipe(in, file);
  file.close();

  return TemporaryDiskFile(file_path);
}

} // namespace odr::internal
