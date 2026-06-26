#pragma once

#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace odr::internal::util::string {

bool starts_with(const std::string &string, const std::string &with);
bool ends_with(const std::string &string, const std::string &with);

/// Predicate deciding whether a byte counts as whitespace for the `*_view`
/// trims. Takes a single `char`; implementations must handle the full byte
/// range without relying on the sign of `char`.
using CharPredicate = bool (*)(char);

/// `std::isspace` for the default C locale, made safe for any `char` value.
bool is_ascii_space(char c);

void ltrim_inplace(std::string &s, CharPredicate is_space = is_ascii_space);
void rtrim_inplace(std::string &s, CharPredicate is_space = is_ascii_space);
void trim_inplace(std::string &s, CharPredicate is_space = is_ascii_space);

std::string ltrim(const std::string &s,
                  CharPredicate is_space = is_ascii_space);
std::string rtrim(const std::string &s,
                  CharPredicate is_space = is_ascii_space);
std::string trim(const std::string &s, CharPredicate is_space = is_ascii_space);

/// Trim leading/trailing whitespace and return a view into `s`. The result is a
/// subrange of `s`, so the leading offset is recoverable as
/// `result.data() - s.data()`.
std::string_view ltrim_view(std::string_view s,
                            CharPredicate is_space = is_ascii_space);
std::string_view rtrim_view(std::string_view s,
                            CharPredicate is_space = is_ascii_space);
std::string_view trim_view(std::string_view s,
                           CharPredicate is_space = is_ascii_space);

void replace_all(std::string &string, const std::string &search,
                 const std::string &replace);

/// Concatenate `count` copies of `unit` (empty for `count == 0`).
std::string repeat(const std::string &unit, std::size_t count);

void split(const std::string &string, const std::string &delimiter,
           const std::function<void(const std::string &)> &callback);
std::vector<std::string> split(const std::string &string,
                               const std::string &delimiter);

std::string to_string(double d, int precision);

std::size_t utf8_length(const std::string &string);

std::string u16string_to_string(const std::u16string &string);
std::u16string string_to_u16string(const std::string &string);
std::string c16str_to_string(const char16_t *c16str, std::size_t length);
void append_c32(char32_t c, std::string &string);

} // namespace odr::internal::util::string
