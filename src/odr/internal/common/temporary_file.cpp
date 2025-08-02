#include <odr/internal/common/temporary_file.hpp>

#include <odr/internal/common/random.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <fstream>
#include <utility>

namespace odr::internal {

TemporaryDiskFile::TemporaryDiskFile(const char *path) : DiskFile{path} {}

TemporaryDiskFile::TemporaryDiskFile(const std::string &path)
    : DiskFile{path} {}

TemporaryDiskFile::TemporaryDiskFile(AbsPath path)
    : DiskFile{std::move(path)} {}

TemporaryDiskFile::TemporaryDiskFile(const TemporaryDiskFile &) = default;

TemporaryDiskFile::TemporaryDiskFile(TemporaryDiskFile &&) noexcept = default;

TemporaryDiskFile::~TemporaryDiskFile() {
  std::filesystem::remove(disk_path()->string());
}

TemporaryDiskFile &
TemporaryDiskFile::operator=(const TemporaryDiskFile &) = default;

TemporaryDiskFile &
TemporaryDiskFile::operator=(TemporaryDiskFile &&) noexcept = default;

const TemporaryDiskFileFactory &TemporaryDiskFileFactory::system_default() {
  static TemporaryDiskFileFactory instance(
      AbsPath(std::filesystem::temp_directory_path()),
      default_random_file_name_generator());
  return instance;
}

TemporaryDiskFileFactory::RandomFileNameGenerator
TemporaryDiskFileFactory::default_random_file_name_generator() {
  return []() { return random_string(10); };
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
