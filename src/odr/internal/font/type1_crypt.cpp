#include <odr/internal/font/type1_crypt.hpp>

#include <odr/internal/util/string_util.hpp>

#include <cstdint>
#include <optional>
#include <string>

namespace odr::internal::font::type1 {

namespace {

constexpr std::uint16_t c1 = 52845;
constexpr std::uint16_t c2 = 22719;

/// Hex-decode @p in, skipping whitespace; stops at the first non-hex, non-space
/// byte (the binary `eexec` form never reaches here).
[[nodiscard]] std::string hex_decode(const std::string_view in) {
  std::string out;
  std::int32_t high = -1;
  for (const char ch : in) {
    if (util::string::is_ascii_whitespace(ch)) {
      continue;
    }
    const std::optional<std::uint8_t> value = util::string::hex_digit(ch);
    if (!value) {
      break;
    }
    if (high < 0) {
      high = *value;
    } else {
      out += static_cast<char>((high << 4) | *value);
      high = -1;
    }
  }
  return out;
}

/// Whether @p eexec is the ASCII-hex form: the first four non-space bytes are
/// all hex digits (Type1 spec 7.2 — the binary form is detected as not-this).
[[nodiscard]] bool looks_like_hex(const std::string_view eexec) {
  std::int32_t seen = 0;
  for (const char ch : eexec) {
    if (util::string::is_ascii_whitespace(ch)) {
      continue;
    }
    if (!util::string::hex_digit(ch)) {
      return false;
    }
    if (++seen == 4) {
      return true;
    }
  }
  return false;
}

} // namespace

} // namespace odr::internal::font::type1

namespace odr::internal::font {

std::string type1::decrypt(const std::string_view cipher,
                           const std::uint16_t key, const std::size_t skip) {
  std::uint16_t r = key;
  std::string out;
  out.reserve(cipher.size());
  for (const char ch : cipher) {
    const auto c = static_cast<std::uint8_t>(ch);
    out += static_cast<char>(c ^ (r >> 8));
    r = static_cast<std::uint16_t>((std::uint32_t{c} + r) * c1 + c2);
  }
  // A cipher shorter than its prefix decrypts to nothing.
  if (skip >= out.size()) {
    return {};
  }
  return out.substr(skip);
}

std::string type1::decrypt_eexec(const std::string_view eexec) {
  if (looks_like_hex(eexec)) {
    const std::string binary = hex_decode(eexec);
    return decrypt(binary, 55665, 4);
  }
  return decrypt(eexec, 55665, 4);
}

std::string type1::decrypt_charstring(const std::string_view charstring,
                                      const std::size_t len_iv) {
  return decrypt(charstring, 4330, len_iv);
}

} // namespace odr::internal::font
