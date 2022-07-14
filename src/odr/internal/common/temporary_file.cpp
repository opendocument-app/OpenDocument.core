#include <fstream>
#include <odr/internal/common/random.hpp>
#include <odr/internal/common/temporary_file.hpp>
#include <odr/internal/util/stream_util.hpp>

namespace odr::internal::common {

TemporaryDiskFile::TemporaryDiskFile(const char *path) : DiskFile{path} {}

TemporaryDiskFile::TemporaryDiskFile(std::string path)
    : DiskFile{std::move(path)} {}

TemporaryDiskFile::TemporaryDiskFile(common::Path path)
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
      std::filesystem::temp_directory_path());
  return instance;
}

TemporaryDiskFileFactory::RandomFileNameGenerator
TemporaryDiskFileFactory::default_random_file_name_generator() {
  return []() { return random_string(10); };
}

TemporaryDiskFileFactory::TemporaryDiskFileFactory(
    common::Path directory, RandomFileNameGenerator random_file_name_generator)
    : m_directory{std::move(directory)}, m_random_file_name_generator{
                                             random_file_name_generator} {}

TemporaryDiskFile
TemporaryDiskFileFactory::copy(const abstract::File &file) const {
  return copy(*file.stream());
}

TemporaryDiskFile TemporaryDiskFileFactory::copy(std::istream &in) const {
  std::fstream file;
  Path file_path;

  while (true) {
    std::string file_name = m_random_file_name_generator();
    file_path = m_directory.join(file_name);

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

} // namespace odr::internal::common
