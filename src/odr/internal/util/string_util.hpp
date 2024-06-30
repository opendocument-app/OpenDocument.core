#ifndef ODR_INTERNAL_STRING_UTIL_HPP
#define ODR_INTERNAL_STRING_UTIL_HPP

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace odr::internal::util::string {
bool starts_with(const std::string &string, const std::string &with);
bool ends_with(const std::string &string, const std::string &with);

void ltrim(std::string &s);
void rtrim(std::string &s);
void trim(std::string &s);

void replace_all(std::string &string, const std::string &search,
                 const std::string &replace);

void split(const std::string &string, const std::string &delimiter,
           std::function<void(const std::string &)> callback);
std::vector<std::string> split(const std::string &string,
                               const std::string &delimiter);

std::string to_string(double d, std::uint32_t precision);

std::string u16string_to_string(const std::u16string &string);
std::u16string string_to_u16string(const std::string &string);
std::string c16str_to_string(const char16_t *c16str, std::size_t length);
} // namespace odr::internal::util::string

#endif // ODR_INTERNAL_STRING_UTIL_HPP
