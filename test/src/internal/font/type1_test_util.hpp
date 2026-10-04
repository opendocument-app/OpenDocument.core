#pragma once

#include <cstdint>
#include <string>
#include <string_view>

namespace odr::test::font {

/// Reference encryption for Type1 fixtures; the library implements decryption.
inline std::string encrypt(const std::string &plain, std::uint16_t state,
                           const std::string &prefix) {
  std::string out;
  for (const char ch : prefix + plain) {
    const auto cipher =
        static_cast<std::uint8_t>(static_cast<std::uint8_t>(ch) ^ (state >> 8));
    out += static_cast<char>(cipher);
    state = static_cast<std::uint16_t>(
        (std::uint32_t{cipher} + state) * 52845U + 22719U);
  }
  return out;
}

} // namespace odr::test::font
