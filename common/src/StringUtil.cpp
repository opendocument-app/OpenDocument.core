#include <common/StringUtil.h>

namespace odr {
namespace common {

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

} // namespace common
} // namespace odr
