#include <odr/internal/util/byte_util.hpp>

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

} // namespace odr::internal::util
