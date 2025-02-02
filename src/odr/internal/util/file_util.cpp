#include <odr/internal/util/file_util.hpp>

#include <odr/exceptions.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <fstream>
#include <iterator>

namespace odr::internal::util {

std::size_t file::size(const std::string &path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  if (!in.is_open() || in.fail()) {
    throw FileNotFound();
  }
  return in.tellg();
}

std::string file::read(const std::string &path) {
  std::ifstream in(path);
  if (!in.is_open() || in.fail()) {
    throw FileNotFound();
  }

  std::string result;

  try {
    in.seekg(0, std::ios::end);
    result.reserve(in.tellg());
    in.seekg(0, std::ios::beg);

    result.assign(std::istreambuf_iterator<char>(in),
                  std::istreambuf_iterator<char>());
  } catch (std::exception &e) {
    // TODO
    throw e;
  }

  return result;
}

void file::write(const std::string &path, const std::string &data) {
  std::ofstream out(path);
  if (!out.is_open() || out.fail()) {
    throw FileNotFound();
  }

  out << data;
}

void file::write(const std::string &path, const abstract::File &file) {
  std::ofstream out(path);
  if (!out.is_open() || out.fail()) {
    throw FileNotFound();
  }

  stream::pipe(*file.stream(), out);
}

} // namespace odr::internal::util
