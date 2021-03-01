#ifndef ODR_STRING_UTIL_H
#define ODR_STRING_UTIL_H

#include <string>

namespace odr::util::string {
bool starts_with(const std::string &string, const std::string &with);
bool ends_with(const std::string &string, const std::string &with);
void replace_all(std::string &string, const std::string &search,
                 const std::string &replace);

std::string to_string(double d, std::uint32_t precision);

std::string u16string_to_string(const std::u16string &string);
std::u16string string_to_u16string(const std::string &string);
std::string c16str_to_string(const char16_t *c16str, std::size_t length);
} // namespace odr::util::string

#endif // ODR_STRING_UTIL_H
