#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
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

bool string::is_ascii_space(const char c) {
  return std::isspace(static_cast<std::uint8_t>(c)) != 0;
}

void string::ltrim_inplace(std::string &s, const CharPredicate is_space) {
  s.erase(s.begin(), std::ranges::find_if(s, [is_space](const char ch) {
            return !is_space(ch);
          }));
}

void string::rtrim_inplace(std::string &s, const CharPredicate is_space) {
  s.erase(std::find_if(s.rbegin(), s.rend(),
                       [is_space](const char ch) { return !is_space(ch); })
              .base(),
          s.end());
}

void string::trim_inplace(std::string &s, const CharPredicate is_space) {
  rtrim_inplace(s, is_space);
  ltrim_inplace(s, is_space);
}

std::string string::ltrim(const std::string &s, const CharPredicate is_space) {
  return std::string(ltrim_view(s, is_space));
}

std::string string::rtrim(const std::string &s, const CharPredicate is_space) {
  return std::string(rtrim_view(s, is_space));
}

std::string string::trim(const std::string &s, const CharPredicate is_space) {
  return std::string(trim_view(s, is_space));
}

std::string_view string::ltrim_view(std::string_view s,
                                    const CharPredicate is_space) {
  std::size_t begin = 0;
  while (begin < s.size() && is_space(s[begin])) {
    ++begin;
  }
  return s.substr(begin);
}

std::string_view string::rtrim_view(std::string_view s,
                                    const CharPredicate is_space) {
  std::size_t end = s.size();
  while (end > 0 && is_space(s[end - 1])) {
    --end;
  }
  return s.substr(0, end);
}

std::string_view string::trim_view(std::string_view s,
                                   const CharPredicate is_space) {
  return ltrim_view(rtrim_view(s, is_space), is_space);
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
  std::size_t last_end = 0;
  while (true) {
    const std::size_t pos = string.find(delimiter, last_end);
    if (pos == std::string::npos) {
      break;
    }
    callback(string.substr(last_end, pos - last_end));
    last_end = pos + delimiter.size();
  }
  callback(string.substr(last_end));
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

void string::append_c32(const char32_t c, std::string &string) {
  utf8::append(c, string);
}

} // namespace odr::internal::util
