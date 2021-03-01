#include <codecvt>
#include <iomanip>
#include <locale>
#include <sstream>
#include <util/string_util.h>

namespace odr::util {

bool string::starts_with(const std::string &string, const std::string &with) {
  return string.rfind(with, 0) == 0;
}

bool string::ends_with(const std::string &string, const std::string &with) {
  return (string.length() >= with.length()) &&
         (string.compare(string.length() - with.length(), with.length(),
                         with) == 0);
}

void string::replace_all(std::string &string, const std::string &search,
                         const std::string &replace) {
  std::size_t pos = string.find(search);
  while (pos != std::string::npos) {
    string.replace(pos, search.size(), replace);
    pos = string.find(search, pos + replace.size());
  }
}

std::string string::to_string(const double d, const std::uint32_t precision) {
  std::stringstream stream;
  stream << std::fixed << std::setprecision(precision) << d;
  return stream.str();
}

std::string string::u16string_to_string(const std::u16string &string) {
  static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>
      convert;
  return convert.to_bytes(string);
}

std::u16string string::string_to_u16string(const std::string &string) {
  static std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t>
      convert;
  return convert.from_bytes(string);
}

std::string string::c16str_to_string(const char16_t *c16str,
                                     std::size_t length) {
  return u16string_to_string(std::u16string(c16str, length / 2));
}

} // namespace odr::util
