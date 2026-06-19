#pragma once

#include <cassert>
#include <memory>
#include <ranges>
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

template <typename I, std::ranges::random_access_range O>
void to_little_endian(I in, O &out) {
  assert(std::ranges::size(out) >= sizeof(in) &&
         "output range too small for input type");
  for (unsigned int i = 0; i < sizeof(in); ++i) {
    out[i] = in & 0xff;
    in >>= 8;
  }
}

template <typename I, std::ranges::random_access_range O>
void to_big_endian(I in, O &out) {
  assert(std::ranges::size(out) >= sizeof(in) &&
         "output range too small for input type");
  for (int i = sizeof(in) - 1; i >= 0; --i) {
    out[i] = in & 0xff;
    in >>= 8;
  }
}

template <std::ranges::random_access_range I, typename O>
void from_little_endian(const I &in, O &out) {
  assert(std::ranges::size(in) >= sizeof(out) &&
         "input range too small for output type");
  out = 0;
  for (unsigned int i = 0; i < sizeof(out); ++i) {
    out |= (static_cast<O>(in[i]) & 0xff) << (i * 8);
  }
}

template <typename O, std::ranges::random_access_range I>
O from_little_endian(const I &in) {
  O out{};
  from_little_endian(in, out);
  return out;
}

template <std::ranges::random_access_range I, typename O>
void from_big_endian(const I &in, O &out) {
  assert(std::ranges::size(in) >= sizeof(out) &&
         "input range too small for output type");
  out = 0;
  for (unsigned int i = 0; i < sizeof(out); ++i) {
    out = (out << 8) | (static_cast<O>(in[i]) & 0xff);
  }
}

template <typename O, std::ranges::random_access_range I>
O from_big_endian(const I &in) {
  O out{};
  from_big_endian(in, out);
  return out;
}

std::string xor_bytes(const std::string &a, const std::string &b);

} // namespace odr::internal::util::byte
