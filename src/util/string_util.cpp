#include <iomanip>
#include <sstream>
#include <util/string_util.h>

namespace odr::util {

bool string::starts_with(const std::string &string, const std::string &with) {
  return string.rfind(with, 0) == 0;
}

bool string::ends_with(const std::string &string, const std::string &with) {
  return (string.length() >= with.length()) &&
         (string.compare(string.length() - with.length(), with.length(),
                         with) == 0);
}

void string::replace_all(std::string &string, const std::string &search,
                         const std::string &replace) {
  std::size_t pos = string.find(search);
  while (pos != std::string::npos) {
    string.replace(pos, search.size(), replace);
    pos = string.find(search, pos + replace.size());
  }
}

std::string string::to_string(const double d, const std::uint32_t precision) {
  std::stringstream stream;
  stream << std::fixed << std::setprecision(precision) << d;
  return stream.str();
}

} // namespace odr::util
