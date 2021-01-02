#include <cerrno>
#include <common/file_util.h>
#include <cstring>
#include <fstream>
#include <odr/exceptions.h>
#include <streambuf>

namespace odr::common {

std::string FileUtil::read(const std::string &path) {
  std::ifstream in(path);
  if (!in.is_open() || in.fail()) {
    throw FileNotFound(std::strerror(errno));
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

} // namespace odr::common
