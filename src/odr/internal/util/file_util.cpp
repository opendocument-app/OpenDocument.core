#include <odr/internal/util/file_util.hpp>

#include <odr/exceptions.hpp>

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

} // namespace odr::internal::util
