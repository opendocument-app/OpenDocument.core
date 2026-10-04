#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace odr::internal::font::type1 {

/// Decrypt @p cipher with the running-key cipher seeded at @p key, discarding
/// the first @p skip plaintext bytes.
[[nodiscard]] std::string decrypt(std::string_view cipher, std::uint16_t key,
                                  std::size_t skip);

/// Decrypt binary or ASCII-hex eexec data (Adobe Type 1 Font Format 7.2).
[[nodiscard]] std::string decrypt_eexec(std::string_view eexec);

/// Decrypt one charstring (key 4330), discarding @p len_iv leading bytes
/// (the font's `/lenIV`, default 4).
[[nodiscard]] std::string decrypt_charstring(std::string_view charstring,
                                             std::size_t len_iv = 4);

} // namespace odr::internal::font::type1
