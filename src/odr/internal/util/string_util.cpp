#include <odr/internal/util/string_util.hpp>

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <stdexcept>

#include <utf8/unchecked.h>
#include <utf8cpp/utf8/cpp17.h>

namespace odr::internal::util {

bool string::starts_with(const std::string &string, const std::string &with) {
  return string.starts_with(with);
}

bool string::ends_with(const std::string &string, const std::string &with) {
  return string.ends_with(with);
}

bool string::is_ascii_space(const char c) {
  return std::isspace(static_cast<std::uint8_t>(c)) != 0;
}

char string::to_lower(const char c) {
  return static_cast<char>(std::tolower(static_cast<std::uint8_t>(c)));
}

std::string string::to_lower(const std::string_view string) {
  std::string result;
  result.reserve(string.size());
  std::ranges::transform(string, std::back_inserter(result),
                         [](const char c) { return to_lower(c); });
  return result;
}

bool string::equals_ignore_case(const std::string_view a,
                                const std::string_view b) {
  return std::ranges::equal(a, b, [](const char x, const char y) {
    return to_lower(x) == to_lower(y);
  });
}

bool string::starts_with_ignore_case(const std::string_view string,
                                     const std::string_view prefix) {
  return string.size() >= prefix.size() &&
         equals_ignore_case(string.substr(0, prefix.size()), prefix);
}

std::size_t string::find_ignore_case(const std::string_view string,
                                     const std::string_view needle,
                                     const std::size_t from) {
  if (from > string.size()) {
    return std::string_view::npos;
  }
  const std::string_view rest = string.substr(from);
  const auto found =
      std::ranges::search(rest, needle, [](const char x, const char y) {
        return to_lower(x) == to_lower(y);
      });
  if (found.empty()) {
    return std::string_view::npos;
  }
  return from + static_cast<std::size_t>(found.begin() - rest.begin());
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

std::string string::repeat(const std::string &unit, const std::size_t count) {
  std::string result;
  result.reserve(unit.size() * count);
  for (std::size_t i = 0; i < count; ++i) {
    result += unit;
  }
  return result;
}

void string::split(const std::string &string, const std::string &delimiter,
                   const std::function<void(const std::string &)> &callback) {
  // an empty delimiter never advances the scan, so it would loop forever
  if (delimiter.empty()) {
    throw std::invalid_argument("delimiter must not be empty");
  }

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

std::size_t string::utf8_length(const std::string &string) {
  return utf8::unchecked::distance(string.begin(), string.end());
}

std::string string::u16string_to_string(const std::u16string &string) {
  return utf8::utf16to8(string);
}

std::u16string string::string_to_u16string(const std::string_view string) {
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
