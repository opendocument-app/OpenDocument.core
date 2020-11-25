#ifndef ODR_COMMON_STRINGUTIL_H
#define ODR_COMMON_STRINGUTIL_H

#include <string>

namespace odr::common::StringUtil {
bool startsWith(const std::string &string, const std::string &with);
bool endsWith(const std::string &string, const std::string &with);
void findAndReplaceAll(std::string &string, const std::string &search,
                       const std::string &replace);
} // namespace odr::common::StringUtil

#endif // ODR_COMMON_STRINGUTIL_H
