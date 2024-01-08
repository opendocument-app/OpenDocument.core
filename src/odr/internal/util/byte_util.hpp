#ifndef ODR_INTERNAL_BYTE_UTIL_HPP
#define ODR_INTERNAL_BYTE_UTIL_HPP

#include <memory>
#include <string>

namespace odr::internal::util {

template <class T> void reverse_bytes(T &x) {
  for (char *a = reinterpret_cast<char *>(std::addressof(x)),
            *b = a + sizeof(x) - 1;
       a < b; ++a, --b) {
    std::swap(*a, *b);
  }
}

void reverse_bytes(char16_t *string, std::size_t length);
void reverse_bytes(char32_t *string, std::size_t length);
void reverse_bytes(std::u16string &string);
void reverse_bytes(std::u32string &string);

} // namespace odr::internal::util

#endif // ODR_INTERNAL_BYTE_UTIL_HPP
