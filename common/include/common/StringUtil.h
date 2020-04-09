#ifndef ODR_COMMON_STRINGUTIL_H
#define ODR_COMMON_STRINGUTIL_H

#include <string>

namespace odr {
namespace common {

namespace StringUtil {
bool startsWith(const std::string &string, const std::string &with);
bool endsWith(const std::string &string, const std::string &with);
void findAndReplaceAll(std::string &string, const std::string &search,
                       const std::string &replace);
} // namespace StringUtil

} // namespace common
} // namespace odr

#endif // ODR_COMMON_STRINGUTIL_H
