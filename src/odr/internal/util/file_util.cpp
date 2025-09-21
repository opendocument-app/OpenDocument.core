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
  return in.tellg();
}

std::string file::read(const std::string &path) {
  std::ifstream in = open(path);

  std::string result;

  in.seekg(0, std::ios::end);
  result.reserve(in.tellg());
  in.seekg(0, std::ios::beg);

  result.assign(std::istreambuf_iterator(in), std::istreambuf_iterator<char>());

  return result;
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
}

void file::write(std::istream &in, const std::string &path) {
  std::ofstream out = create(path);
  stream::pipe(in, out);
}

} // namespace odr::internal::util
