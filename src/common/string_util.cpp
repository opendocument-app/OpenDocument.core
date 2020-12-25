#include <common/string_util.h>
#include <iomanip>
#include <sstream>

namespace odr::common {

bool StringUtil::startsWith(const std::string &string,
                            const std::string &with) {
  return string.rfind(with, 0) == 0;
}

bool StringUtil::endsWith(const std::string &string, const std::string &with) {
  return (string.length() >= with.length()) &&
         (string.compare(string.length() - with.length(), with.length(),
                         with) == 0);
}

void StringUtil::findAndReplaceAll(std::string &string,
                                   const std::string &search,
                                   const std::string &replace) {
  std::size_t pos = string.find(search);
  while (pos != std::string::npos) {
    string.replace(pos, search.size(), replace);
    pos = string.find(search, pos + replace.size());
  }
}

std::string StringUtil::toString(const double d,
                                 const std::uint32_t precision) {
  std::stringstream stream;
  stream << std::fixed << std::setprecision(precision) << d;
  return stream.str();
}

} // namespace odr::common
