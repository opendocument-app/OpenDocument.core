#include <odr/internal/ooxml/ooxml_crypto.hpp>

#include <odr/exceptions.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>

using namespace odr::internal::ooxml;

TEST(OoxmlCrypto, ECMA376Standard_derive_key) {
  crypto::EncryptionHeader encryption_header{};
  encryption_header.flags = 0x24;
  encryption_header.alg_id = 0x660e;
  encryption_header.alg_id_hash = 0x8004;
  encryption_header.key_size = 128;
  encryption_header.provider_type = 0x18;
  crypto::EncryptionVerifier encryption_verifier{};
  encryption_verifier.salt_size = 16;
  encryption_verifier.verifier_hash_size = 20;
  std::memcpy(encryption_verifier.salt.data(),
              "\xe8\x82"
              "fI\x0c[\xd1\xee\xbd+C\x94\xe3\xf8"
              "0\xef",
              sizeof(encryption_verifier.salt));
  const std::string password = "Password1234_";
  const std::string expected_key =
      "@\xb1:q\xf9\x0b\x96n7T\x08\xf2\xd1\x81\xa1\xaa";

  const crypto::ECMA376Standard crypto(encryption_header, encryption_verifier,
                                       std::string(32, '\0'));
  const std::string key = crypto.derive_key(password);

  EXPECT_EQ(expected_key, key);
}

TEST(OoxmlCrypto, ECMA376Standard_verify) {
  crypto::EncryptionHeader encryption_header{};
  encryption_header.flags = 0x24;
  encryption_header.alg_id = 0x660e;
  encryption_header.alg_id_hash = 0x8004;
  encryption_header.key_size = 128;
  crypto::EncryptionVerifier encryption_verifier{};
  encryption_verifier.salt_size = 16;
  encryption_verifier.verifier_hash_size = 20;
  std::memcpy(encryption_verifier.encrypted_verifier.data(),
              "Qos.\x96o\xac\x17\xb1\xc5\xd7\xd8\xcc"
              "6\xc9(",
              sizeof(encryption_verifier.encrypted_verifier));
  const std::string encrypted_verifier_hash =
      "+ah\xda\xbe)\x11\xad+\xd3|\x17"
      "Ft\\\x14\xd3\xcf\x1b\xb1@\xa4\x8fNo=#\x88\x08r\xb1j";
  const std::string key = "@\xb1:q\xf9\x0b\x96n7T\x08\xf2\xd1\x81\xa1\xaa";

  const crypto::ECMA376Standard crypto(encryption_header, encryption_verifier,
                                       encrypted_verifier_hash);
  EXPECT_TRUE(crypto.verify(key));
}

TEST(OoxmlCrypto, validates_standard_encryption_fields_and_package_size) {
  crypto::EncryptionHeader header{};
  header.flags = 0x24;
  header.alg_id = 0x660e;
  header.alg_id_hash = 0x8004;
  header.key_size = 128;
  crypto::EncryptionVerifier verifier{};
  verifier.salt_size = 16;
  verifier.verifier_hash_size = 20;
  const std::string hash(32, '\0');
  std::string info;
  const auto append = [&info](const auto &value) {
    info.append(reinterpret_cast<const char *>(&value), sizeof(value));
  };
  append(crypto::VersionInfo{4, 2});
  append(crypto::StandardHeader{header.flags, sizeof(header)});
  append(header);
  append(verifier);
  info += hash;
  EXPECT_NO_THROW((void)crypto::Util(info));
  EXPECT_NO_THROW((void)crypto::Util(info + "trailing"));
  EXPECT_THROW((void)crypto::Util(info.substr(0, info.size() - 1)),
               std::runtime_error);
  std::fill_n(info.begin() + 8, 4, '\xff');
  EXPECT_THROW((void)crypto::Util(info), std::runtime_error);
  const crypto::ECMA376Standard algorithm(header, verifier, hash);
  const std::string key(16, 'k');
  const std::string ciphertext = odr::internal::crypto::util::encrypt_aes_cbc(
      key, std::string(16, '\0'), "plaintext.......");
  const auto package = [&](const std::uint64_t size) {
    std::string result;
    odr::internal::util::byte_string::put_u32_le(
        result, static_cast<std::uint32_t>(size));
    odr::internal::util::byte_string::put_u32_le(
        result, static_cast<std::uint32_t>(size >> 32));
    return result + ciphertext;
  };
  EXPECT_EQ("plaintext", algorithm.decrypt(package(9), key));
  EXPECT_THROW((void)algorithm.decrypt(package(17), key), std::runtime_error);
  EXPECT_THROW((void)algorithm.decrypt(package(0x100000009ULL), key),
               std::runtime_error);
  header.key_size = 129;
  EXPECT_THROW((void)crypto::ECMA376Standard(header, verifier, hash),
               odr::MsUnsupportedCryptoAlgorithm);
  header.key_size = 128;
  header.alg_id_hash = 0;
  EXPECT_THROW((void)crypto::ECMA376Standard(header, verifier, hash),
               odr::MsUnsupportedCryptoAlgorithm);
  header.alg_id_hash = 0x8004;
  verifier.verifier_hash_size = 16;
  EXPECT_THROW((void)crypto::ECMA376Standard(header, verifier, hash),
               std::runtime_error);
  verifier.verifier_hash_size = 20;
  EXPECT_THROW((void)crypto::ECMA376Standard(header, verifier, hash.substr(1)),
               std::runtime_error);
}
