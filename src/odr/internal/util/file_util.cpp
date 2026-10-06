#include <odr/internal/util/file_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/temporary_file.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <fstream>
#include <iterator>

namespace odr::internal::util {

std::ifstream file::open(const std::string &path) {
  std::ifstream in(path, std::ifstream::binary);
  if (!in.is_open() || in.fail()) {
    throw FileNotFound(path);
  }
  return in;
}

std::size_t file::size(const std::string &path) {
  std::ifstream in = open(path);
  in.seekg(0, std::ios::end);
  const auto end = in.tellg();
  if (end < 0) {
    throw FileReadError();
  }
  return static_cast<std::size_t>(end);
}

std::string file::read(const std::string &path) {
  std::ifstream in = open(path);

  return stream::read(in);
}

void file::pipe(const std::string &path, std::ostream &out) {
  std::ifstream in = open(path);
  stream::pipe(in, out);
}

std::ofstream file::create(const std::string &path) {
  std::ofstream out(path, std::ifstream::binary);
  if (!out.is_open() || out.fail()) {
    throw FileWriteError(path);
  }
  return out;
}

void file::write_atomic(const std::string &path,
                        const std::function<void(std::ostream &)> &write) {
  const std::filesystem::path target =
      std::filesystem::weakly_canonical(std::filesystem::absolute(path));
  const auto status = std::filesystem::status(target);
  // a rename needs only the directory to be writable, so the file's own
  // permissions refuse as truncating it would
  if (std::filesystem::is_symlink(std::filesystem::symlink_status(target)) ||
      (std::filesystem::exists(status) &&
       (!std::filesystem::is_regular_file(status) ||
        (status.permissions() & std::filesystem::perms::owner_write) ==
            std::filesystem::perms::none))) {
    throw FileWriteError(path);
  }
  TemporaryDiskFileFactory(AbsPath(target.parent_path()))
      .create(write)
      .persist(AbsPath(target));
}

void file::write(const std::string &data, const std::string &path) {
  std::ofstream out = create(path);
  out << data;
  out.close();
  if (!out) {
    throw FileWriteError(path);
  }
}

void file::write(std::istream &in, const std::string &path) {
  std::ofstream out = create(path);
  stream::pipe(in, out);
  out.close();
  if (!out) {
    throw FileWriteError(path);
  }
}

} // namespace odr::internal::util
