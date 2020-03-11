#ifndef ODR_STRINGUTIL_H
#define ODR_STRINGUTIL_H

#include <string>

namespace odr {

namespace StringUtil {
extern bool startsWith(const std::string &string, const std::string &with);
extern bool endsWith(const std::string &string, const std::string &with);
extern void findAndReplaceAll(std::string &string, const std::string &search,
                              const std::string &replace);
} // namespace StringUtil

} // namespace odr

#endif // ODR_STRINGUTIL_H
