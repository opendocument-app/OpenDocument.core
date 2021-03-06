#include <cstring>
#include <gtest/gtest.h>
#include <ooxml/ooxml_crypto.h>
#include <string>

using namespace odr::ooxml;

TEST(OoxmlCrypto, ECMA376Standard_derive_key) {
  Crypto::EncryptionHeader encryption_header{};
  encryption_header.alg_id = 0x660e;
  encryption_header.alg_id_hash = 0x8004;
  encryption_header.key_size = 128;
  encryption_header.provider_type = 0x18;
  Crypto::EncryptionVerifier encryption_verifier{};
  encryption_verifier.salt_size = 16;
  std::memcpy(encryption_verifier.salt,
              "\xe8\x82"
              "fI\x0c[\xd1\xee\xbd+C\x94\xe3\xf8"
              "0\xef",
              sizeof(encryption_verifier.salt));
  const std::string password = "Password1234_";
  const std::string expected_key =
      "@\xb1:q\xf9\x0b\x96n7T\x08\xf2\xd1\x81\xa1\xaa";

  Crypto::ECMA376Standard crypto(encryption_header, encryption_verifier, "");
  const std::string key = crypto.derive_key(password);

  EXPECT_EQ(expected_key, key);
}

TEST(OoxmlCrypto, ECMA376Standard_verify) {
  const Crypto::EncryptionHeader encryption_header{};
  Crypto::EncryptionVerifier encryption_verifier{};
  encryption_verifier.verifierHashSize = 16;
  std::memcpy(encryption_verifier.encrypted_verifier,
              "Qos.\x96o\xac\x17\xb1\xc5\xd7\xd8\xcc"
              "6\xc9(",
              sizeof(encryption_verifier.encrypted_verifier));
  const std::string encrypted_verifier_hash =
      "+ah\xda\xbe)\x11\xad+\xd3|\x17"
      "Ft\\\x14\xd3\xcf\x1b\xb1@\xa4\x8fNo=#\x88\x08r\xb1j";
  const std::string key = "@\xb1:q\xf9\x0b\x96n7T\x08\xf2\xd1\x81\xa1\xaa";

  Crypto::ECMA376Standard crypto(encryption_header, encryption_verifier,
                                 encrypted_verifier_hash);
  EXPECT_TRUE(crypto.verify(key));
}
