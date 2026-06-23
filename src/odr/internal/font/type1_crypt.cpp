#include <odr/internal/font/type1_crypt.hpp>

#include <cctype>
#include <cstdint>
#include <string>

namespace odr::internal::font::type1 {

namespace {

constexpr std::uint16_t c1 = 52845;
constexpr std::uint16_t c2 = 22719;

[[nodiscard]] bool is_hex_digit(const unsigned char c) {
  return std::isxdigit(c) != 0;
}

/// Hex-decode @p in, skipping whitespace; stops at the first non-hex, non-space
/// byte (the binary `eexec` form never reaches here).
[[nodiscard]] std::string hex_decode(std::string_view in) {
  std::string out;
  int high = -1;
  for (const char ch : in) {
    const auto c = static_cast<unsigned char>(ch);
    if (std::isspace(c) != 0) {
      continue;
    }
    if (!is_hex_digit(c)) {
      break;
    }
    const int value = (c <= '9')   ? c - '0'
                      : (c <= 'F') ? c - 'A' + 10
                                   : c - 'a' + 10;
    if (high < 0) {
      high = value;
    } else {
      out += static_cast<char>((high << 4) | value);
      high = -1;
    }
  }
  return out;
}

/// Whether @p eexec is the ASCII-hex form: the first four non-space bytes are
/// all hex digits (Type1 spec 7.2 — the binary form is detected as not-this).
[[nodiscard]] bool looks_like_hex(std::string_view eexec) {
  int seen = 0;
  for (const char ch : eexec) {
    const auto c = static_cast<unsigned char>(ch);
    if (std::isspace(c) != 0) {
      continue;
    }
    if (!is_hex_digit(c)) {
      return false;
    }
    if (++seen == 4) {
      return true;
    }
  }
  return false;
}

} // namespace

std::string decrypt(const std::string_view cipher, const std::uint16_t key,
                    const std::size_t skip) {
  std::uint16_t r = key;
  std::string out;
  out.reserve(cipher.size());
  for (const char ch : cipher) {
    const auto c = static_cast<std::uint8_t>(ch);
    out += static_cast<char>(c ^ (r >> 8));
    r = static_cast<std::uint16_t>((c + r) * c1 + c2);
  }
  if (skip >= out.size()) {
    return {};
  }
  return out.substr(skip);
}

std::string decrypt_eexec(const std::string_view eexec) {
  if (looks_like_hex(eexec)) {
    const std::string binary = hex_decode(eexec);
    return decrypt(binary, 55665, 4);
  }
  return decrypt(eexec, 55665, 4);
}

std::string decrypt_charstring(const std::string_view charstring,
                               const std::size_t len_iv) {
  return decrypt(charstring, 4330, len_iv);
}

} // namespace odr::internal::font::type1
