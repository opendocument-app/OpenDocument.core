#include <odr/internal/font/type1_crypt.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

using namespace odr::internal::font::type1;

namespace {

/// Independent reference implementation of the Type1 *encryption* (the inverse
/// of `decrypt`), so the round-trip tests are not circular: this codes the
/// cipher forwards (plaintext -> ciphertext), `decrypt` codes it backwards.
std::string encrypt(const std::string &plain, std::uint16_t r,
                    const std::string &random_prefix) {
  constexpr std::uint16_t c1 = 52845;
  constexpr std::uint16_t c2 = 22719;
  std::string out;
  const std::string full = random_prefix + plain;
  for (const char ch : full) {
    const auto p = static_cast<std::uint8_t>(ch);
    const auto cipher = static_cast<std::uint8_t>(p ^ (r >> 8));
    out += static_cast<char>(cipher);
    r = static_cast<std::uint16_t>((cipher + r) * c1 + c2);
  }
  return out;
}

} // namespace

TEST(Type1CryptTest, EexecRoundTrip) {
  const std::string plain = "/Private 10 dict dup begin";
  const std::string cipher = encrypt(plain, 55665, "ABCD");
  EXPECT_EQ(decrypt_eexec(cipher), plain);
}

TEST(Type1CryptTest, CharstringRoundTrip) {
  const std::string plain("\x0d\x0e\xff\x00\x01", 5); // hsbw-ish bytes
  const std::string cipher = encrypt(plain, 4330, "wxyz");
  EXPECT_EQ(decrypt_charstring(cipher), plain); // default lenIV = 4
}

TEST(Type1CryptTest, CharstringHonoursLenIv) {
  const std::string plain = "hello";
  const std::string cipher = encrypt(plain, 4330, "");
  EXPECT_EQ(decrypt_charstring(cipher, 0), plain);
}

TEST(Type1CryptTest, EexecAcceptsHexForm) {
  const std::string plain = "dup /CharStrings";
  const std::string binary = encrypt(plain, 55665, "0000");
  // Hex-encode the binary eexec (PFA form), with whitespace the decoder skips.
  std::string hex;
  const char *digits = "0123456789abcdef";
  for (std::size_t i = 0; i < binary.size(); ++i) {
    const auto b = static_cast<std::uint8_t>(binary[i]);
    hex += digits[b >> 4];
    hex += digits[b & 0x0f];
    if (i % 8 == 7) {
      hex += '\n';
    }
  }
  EXPECT_EQ(decrypt_eexec(hex), plain);
}
