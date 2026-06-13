#include <odr/internal/crypto/crypto_util.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using namespace odr::internal::crypto::util;

// RFC 1321, appendix A.5.
TEST(CryptoUtil, md5) {
  EXPECT_EQ(hex_encode(md5("")), "d41d8cd98f00b204e9800998ecf8427e");
  EXPECT_EQ(hex_encode(md5("abc")), "900150983cd24fb0d6963f7d28e17f72");
  EXPECT_EQ(hex_encode(md5("message digest")),
            "f96b697d7cb7938d525a2f31aaf161d0");
}

// FIPS 180-4 example vectors.
TEST(CryptoUtil, sha384) {
  EXPECT_EQ(hex_encode(sha384("abc")),
            "cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed"
            "8086072ba1e7cc2358baeca134c825a7");
}

TEST(CryptoUtil, sha512) {
  EXPECT_EQ(hex_encode(sha512("abc")),
            "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
            "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
}

TEST(CryptoUtil, hex_roundtrip) {
  EXPECT_EQ(hex_encode(std::string("\x00\x48\xff", 3)), "0048ff");
  EXPECT_EQ(hex_decode("0048FF"), std::string("\x00\x48\xff", 3));
  EXPECT_EQ(hex_decode(""), "");
}

// The decoder is strict: it rejects an odd digit count or any non-hex
// character (unlike the lenient ASCIIHexDecode PDF filter).
TEST(CryptoUtil, hex_decode_rejects_bad_input) {
  EXPECT_THROW(hex_decode("abc"), std::invalid_argument);
  EXPECT_THROW(hex_decode("4G"), std::invalid_argument);
  EXPECT_THROW(hex_decode("48 65"), std::invalid_argument);
}

// Well-known RC4 test vector (key "Key", plaintext "Plaintext").
TEST(CryptoUtil, rc4) {
  const std::string cipher = rc4("Key", "Plaintext");
  EXPECT_EQ(hex_encode(cipher), "bbf316e8d940af0ad3");
  // RC4 is symmetric: re-applying the keystream restores the plaintext.
  EXPECT_EQ(rc4("Key", cipher), "Plaintext");
}
