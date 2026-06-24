#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace odr::internal::font::type1 {

// Type1 (Adobe Type 1 Font Format) `eexec` / charstring decryption.
//
// Both the `eexec`-encrypted portion of the font program and each individual
// charstring use the same stream cipher with different keys: a 16-bit running
// key `R`, constants c1 = 52845 / c2 = 22719, where each plaintext byte is
// `cipher ^ (R >> 8)` and `R = (cipher + R) * c1 + c2` (mod 2^16). The leading
// @p skip bytes of plaintext are random padding and discarded.

/// Decrypt @p cipher with the running-key cipher seeded at @p key, discarding
/// the first @p skip plaintext bytes.
[[nodiscard]] std::string decrypt(std::string_view cipher, std::uint16_t key,
                                  std::size_t skip);

/// Decrypt the `eexec` section (key 55665, 4 random bytes). Accepts either the
/// binary form (PDF `/FontFile`, the `/Length2` portion) or the ASCII-hex form
/// (PFA fonts): when the section's leading bytes are all hex digits/whitespace
/// it is hex-decoded first.
[[nodiscard]] std::string decrypt_eexec(std::string_view eexec);

/// Decrypt one charstring (key 4330), discarding @p len_iv leading bytes
/// (the font's `/lenIV`, default 4).
[[nodiscard]] std::string decrypt_charstring(std::string_view charstring,
                                             std::size_t len_iv = 4);

} // namespace odr::internal::font::type1
