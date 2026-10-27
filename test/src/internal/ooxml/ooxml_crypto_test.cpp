#include <odr/internal/ooxml/ooxml_crypto.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/util/byte_string.hpp>
#include <odr/odr.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <tuple>

using namespace odr::internal::ooxml;
using odr::test::TestData;

namespace {

/// The attributes of `CT_KeyData` and `CT_PasswordKeyEncryptor`
/// ([MS-OFFCRYPTO] 2.3.4.10), with an all-zero 16-byte salt.
std::string cipher_attributes(const std::string &cipher) {
  return R"( saltSize="16" blockSize="16" keyBits="256" hashSize="64")"
         R"( cipherAlgorithm=")" +
         cipher +
         R"(" cipherChaining="ChainingModeCBC" hashAlgorithm="SHA512")"
         R"( saltValue="AAAAAAAAAAAAAAAAAAAAAA==")";
}

/// An agile `EncryptionInfo` stream around the given key encryptors.
std::string make_agile_info(const std::string &key_encryptors,
                            const std::string &cipher = "AES") {
  std::string info;
  odr::internal::util::byte_string::put_u32_le(info, 0x00040004);
  odr::internal::util::byte_string::put_u32_le(info, 0x40);
  info +=
      R"(<encryption xmlns="http://schemas.microsoft.com/office/2006/encryption")"
      R"( xmlns:p="http://schemas.microsoft.com/office/2006/keyEncryptor/password">)"
      R"(<keyData)" +
      cipher_attributes(cipher) + R"(/><keyEncryptors>)" + key_encryptors +
      R"(</keyEncryptors></encryption>)";
  return info;
}

std::string make_password_encryptor(const std::string &spin_count) {
  return R"(<keyEncryptor uri="http://schemas.microsoft.com/office/2006/keyEncryptor/password">)"
         R"(<p:encryptedKey spinCount=")" +
         spin_count + R"(")" + cipher_attributes("AES") +
         R"( encryptedVerifierHashInput="AAAAAAAAAAAAAAAAAAAAAA==")"
         R"( encryptedVerifierHashValue="AAAAAAAAAAAAAAAAAAAAAA==")"
         R"( encryptedKeyValue="AAAAAAAAAAAAAAAAAAAAAA=="/></keyEncryptor>)";
}

} // namespace

TEST(OoxmlCrypto, ECMA376Agile_decrypts_word_excel_and_powerpoint) {
  for (const std::string path : {"odr-public/docx/encrypted-agile.docx",
                                 "odr-public/xlsx/encrypted-agile.xlsx",
                                 "odr-public/pptx/encrypted-agile.pptx"}) {
    const odr::DocumentFile file =
        odr::open(TestData::test_file_path(path)).as_document_file();
    EXPECT_EQ(file.encryption_state(), odr::EncryptionState::encrypted) << path;
    EXPECT_THROW(std::ignore = file.decrypt("wrong"), odr::WrongPasswordError)
        << path;

    const odr::DocumentFile decrypted =
        file.decrypt(TestData::test_file(path).password.value());
    EXPECT_EQ(decrypted.encryption_state(), odr::EncryptionState::decrypted)
        << path;
    EXPECT_EQ(decrypted.file_type(), TestData::test_file(path).type) << path;
    EXPECT_TRUE(decrypted.document().root_element().first_child()) << path;
  }
}

TEST(OoxmlCrypto, ECMA376Agile_refuses_what_it_cannot_decrypt) {
  EXPECT_NO_THROW(
      (void)crypto::Util(make_agile_info(make_password_encryptor("100000"))));
  EXPECT_THROW((void)crypto::Util(
                   make_agile_info(make_password_encryptor("100000"), "3DES")),
               odr::MsUnsupportedCryptoAlgorithm);
  // a certificate key encryptor needs a private key, not a password
  EXPECT_THROW(
      (void)crypto::Util(make_agile_info(
          R"(<keyEncryptor uri="http://schemas.microsoft.com/office/2006/keyEncryptor/certificate"/>)")),
      odr::MsUnsupportedCryptoAlgorithm);
  EXPECT_THROW(
      (void)crypto::Util(make_agile_info(make_password_encryptor("10000001"))),
      std::runtime_error);
}

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
