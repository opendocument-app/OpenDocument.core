#ifndef ODR_STRING_UTIL_H
#define ODR_STRING_UTIL_H

#include <string>

namespace odr::util::string {
bool starts_with(const std::string &string, const std::string &with);
bool ends_with(const std::string &string, const std::string &with);
void replace_all(std::string &string, const std::string &search,
                 const std::string &replace);

std::string to_string(double d, std::uint32_t precision);
} // namespace odr::util::string

#endif // ODR_STRING_UTIL_H
