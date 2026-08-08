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

// Argon2id v=19 vectors published by the reference implementation
// (P-H-C/phc-winner-argon2, src/test.c).
TEST(CryptoUtil, argon2id) {
  EXPECT_EQ(hex_encode(argon2id(32, "password", "somesalt", 2, 65536, 1)),
            "09316115d5cf24ed5a15a31a3ba326e5cf32edc24702987c02b6566f61913cf7");
  EXPECT_EQ(hex_encode(argon2id(32, "password", "somesalt", 2, 256, 1)),
            "9dfeb910e80bad0311fee20f9c0e2b12c17987b4cac90c2ef54d5b3021c68bfe");
  EXPECT_EQ(hex_encode(argon2id(32, "password", "somesalt", 2, 256, 2)),
            "6d093c501fd5999645e0ea3bf620d7b8be7fd2db59c20d9fff9539da2bf57037");
}

// Generated with the same reference implementation — its published vectors go
// no further than two lanes.
TEST(CryptoUtil, argon2id_lanes) {
  // What LibreOffice writes for ODF package encryption.
  EXPECT_EQ(
      hex_encode(argon2id(32, "password", "0123456789abcdef", 3, 65536, 4)),
      "b8a64b68dea6b88ca8c8862be706aac37cbecda0db7bd68b48f8fa2e7feb6f3e");
  EXPECT_EQ(hex_encode(argon2id(64, "password", "somesalt", 2, 256, 4)),
            "862f0a0272a6ce8aeb7edf3efabd8287b7dfa4c550207c77471532fec400e46e"
            "a5751a3a8fe4cb2ede8b60ca66de2a8180d3ea39a242d0b4e413f834b9a049ad");
  // Memory is rounded down to a multiple of 4 * lanes.
  EXPECT_EQ(hex_encode(argon2id(32, "password", "somesalt", 2, 37, 3)),
            "fd3d6c0350c90b38be1da55d3387c3da995b683542cf1de6af4cb06f0cbd1188");
}

TEST(CryptoUtil, argon2id_rejects_bad_parameters) {
  EXPECT_THROW(argon2id(32, "password", "short", 2, 256, 1),
               std::invalid_argument);
  EXPECT_THROW(argon2id(32, "password", "somesalt", 2, 32, 8),
               std::invalid_argument);
  EXPECT_THROW(argon2id(32, "password", "somesalt", 0, 256, 1),
               std::invalid_argument);
  EXPECT_THROW(argon2id(2, "password", "somesalt", 2, 256, 1),
               std::invalid_argument);
}

// Well-known RC4 test vector (key "Key", plaintext "Plaintext").
TEST(CryptoUtil, rc4) {
  const std::string cipher = rc4("Key", "Plaintext");
  EXPECT_EQ(hex_encode(cipher), "bbf316e8d940af0ad3");
  // RC4 is symmetric: re-applying the keystream restores the plaintext.
  EXPECT_EQ(rc4("Key", cipher), "Plaintext");
}
