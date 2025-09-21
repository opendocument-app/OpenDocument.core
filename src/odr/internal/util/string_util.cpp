#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <iomanip>
#include <locale>
#include <sstream>

#include <utf8cpp/utf8/cpp17.h>

namespace odr::internal::util {

bool string::starts_with(const std::string &string, const std::string &with) {
  return string.rfind(with, 0) == 0;
}

bool string::ends_with(const std::string &string, const std::string &with) {
  return string.length() >= with.length() &&
         string.compare(string.length() - with.length(), with.length(), with) ==
             0;
}

void string::ltrim(std::string &s) {
  s.erase(s.begin(), std::ranges::find_if(s, [](const std::uint8_t ch) {
            return !std::isspace(ch);
          }));
}

void string::rtrim(std::string &s) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [](const std::uint8_t ch) { return !std::isspace(ch); })
              .base(),
          s.end());
}

void string::trim(std::string &s) {
  rtrim(s);
  ltrim(s);
}

void string::replace_all(std::string &string, const std::string &search,
                         const std::string &replace) {
  std::size_t pos = string.find(search);
  while (pos != std::string::npos) {
    string.replace(pos, search.size(), replace);
    pos = string.find(search, pos + replace.size());
  }
}

void string::split(const std::string &string, const std::string &delimiter,
                   const std::function<void(const std::string &)> &callback) {
  std::size_t pos = 0;
  std::size_t last = 0;
  while ((pos = string.find(delimiter, pos)) != std::string::npos) {
    callback(string.substr(last, pos));
    last = pos;
    pos += delimiter.size();
  }
}

std::vector<std::string> string::split(const std::string &string,
                                       const std::string &delimiter) {
  std::vector<std::string> result;
  split(string, delimiter,
        [&result](const std::string &part) { result.push_back(part); });
  return result;
}

std::string string::to_string(const double d, const int precision) {
  std::stringstream stream;
  stream << std::fixed << std::setprecision(precision) << d;
  return stream.str();
}

std::string string::u16string_to_string(const std::u16string &string) {
  return utf8::utf16to8(string);
}

std::u16string string::string_to_u16string(const std::string &string) {
  return utf8::utf8to16(string);
}

std::string string::c16str_to_string(const char16_t *c16str,
                                     const std::size_t length) {
  return u16string_to_string(std::u16string(c16str, length / 2));
}

} // namespace odr::internal::util
