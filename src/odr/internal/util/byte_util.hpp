#pragma once

#include <memory>
#include <string>

namespace odr::internal::util::byte {

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

template <typename I, std::ranges::range O>
void to_little_endian(I in, O &out) {
  for (int i = 0; i < sizeof(in); ++i) {
    out[i] = in & 0xff;
    in >>= 8;
  }
}

template <typename I, std::ranges::range O> void to_big_endian(I in, O &out) {
  for (int i = sizeof(in) - 1; i >= 0; --i) {
    out[i] = in & 0xff;
    in >>= 8;
  }
}

std::string xor_bytes(const std::string &a, const std::string &b);

} // namespace odr::internal::util::byte
