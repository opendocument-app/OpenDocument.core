#include <odr/internal/util/file_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
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
