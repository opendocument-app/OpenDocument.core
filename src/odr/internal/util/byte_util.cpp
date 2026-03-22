#include <odr/internal/util/byte_util.hpp>

#include <stdexcept>

namespace odr::internal::util {

void byte::reverse_bytes(char16_t *string, const std::size_t length) {
  for (std::size_t i = 0; i < length; ++i) {
    reverse_bytes(string[i]);
  }
}

void byte::reverse_bytes(char32_t *string, const std::size_t length) {
  for (std::size_t i = 0; i < length; ++i) {
    reverse_bytes(string[i]);
  }
}

void byte::reverse_bytes(std::u16string &string) {
  for (char16_t &c : string) {
    reverse_bytes(c);
  }
}

void byte::reverse_bytes(std::u32string &string) {
  for (char32_t &c : string) {
    reverse_bytes(c);
  }
}

std::string byte::xor_bytes(const std::string &a, const std::string &b) {
  if (a.size() != b.size()) {
    throw std::invalid_argument("a.size() != b.size()");
  }

  std::string result(a.size(), ' ');

  for (std::size_t i = 0; i < result.size(); ++i) {
    result[i] = static_cast<char>(a[i] ^ b[i]);
  }

  return result;
}

} // namespace odr::internal::util
