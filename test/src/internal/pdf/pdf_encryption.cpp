#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_encryption.hpp>
#include <odr/internal/pdf/pdf_object.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace odr::internal::pdf;
using namespace odr::internal::crypto::util;

// Known-answer test from the real R 2 / RC4-40 fixture
// (odr-public/pdf/Casio_WVA-M650-7AJF.pdf): derive the file key from /O, /P and
// /ID[0] with the empty user password, then Algorithm 4 must reproduce /U. The
// vectors come from a real producer, so this is not circular through our code.
TEST(PdfEncryption, casio_r2_rc4_user_key) {
  const std::string o = hex_decode(
      "5ee983cbdfb3f64baa265a496c450467891a307fd45c5f7d53282fd1835ce1b1");
  const std::string u = hex_decode(
      "d3f7b55d8d09b0efb6b4c1cc158ea7eaa579f693d93aeccf2d87e99b78376a18");
  const std::string id0 = hex_decode("f2bffd9443e6910523d547b66f58e0c8");

  const std::string key =
      standard_security::compute_key_r2_r4("", o, -60, id0, 2, 5, true);
  EXPECT_EQ(standard_security::compute_u_r2_r4(key, id0, 2), u);
}

// The same fixture, through the public Decryptor API: the empty user password
// authenticates an owner-locked RC4 file.
TEST(PdfEncryption, casio_r2_authenticate_empty) {
  Dictionary encrypt;
  encrypt["Filter"] = Object(Name("Standard"));
  encrypt["R"] = Object(Integer(2));
  encrypt["V"] = Object(Integer(1));
  encrypt["Length"] = Object(Integer(40));
  encrypt["P"] = Object(Integer(-60));
  encrypt["O"] = Object(StandardString(hex_decode(
      "5ee983cbdfb3f64baa265a496c450467891a307fd45c5f7d53282fd1835ce1b1")));
  encrypt["U"] = Object(StandardString(hex_decode(
      "d3f7b55d8d09b0efb6b4c1cc158ea7eaa579f693d93aeccf2d87e99b78376a18")));

  const auto authenticator = Authenticator::create(
      encrypt, hex_decode("f2bffd9443e6910523d547b66f58e0c8"));
  ASSERT_TRUE(authenticator.has_value());
  EXPECT_TRUE(authenticator->authenticate("").has_value());
}

// R 6 / V 5 / AESV3 fixture (odr-private/pdf/encrypted_fontfile3_opentype.pdf);
// the user password ("sample-user-password") is recorded in the fixture
// index.csv. The 2.B hardened hash over the real /U validation salt must
// reproduce the stored /U hash, exercising the whole R 6 password chain.
TEST(PdfEncryption, fontfile3_r6_aesv3) {
  Dictionary encrypt;
  encrypt["Filter"] = Object(Name("Standard"));
  encrypt["R"] = Object(Integer(6));
  encrypt["V"] = Object(Integer(5));
  encrypt["Length"] = Object(Integer(256));
  encrypt["P"] = Object(Integer(-4));
  encrypt["O"] = Object(StandardString(hex_decode(
      "d2549e6679b224b4d049a48571f5293451a389a88358c8f5311d3d11ae001154"
      "5da942eca4591a7eb8d4d5619a750129")));
  encrypt["U"] = Object(StandardString(hex_decode(
      "3691dd81a3d444ad86131ab545c0251d7a352e95c4018810f554259a5a3b2332"
      "3633a7fd430e76322636e7739e4b97c1")));
  encrypt["OE"] = Object(StandardString(hex_decode(
      "98ed9b3c3e14b6456992859d7c752c015479e7c6beb22e8b1f270d2e77e436d3")));
  encrypt["UE"] = Object(StandardString(hex_decode(
      "973fa8deba0ffd1cf8a58783d15532316097831d51ea722465283873c7bf2954")));
  const std::string id0 = hex_decode("48f4ed84fa38cfb32c9641b2442e6a7d");

  const auto authenticator = Authenticator::create(encrypt, id0);
  ASSERT_TRUE(authenticator.has_value());
  EXPECT_TRUE(authenticator->authenticate("sample-user-password").has_value());
  EXPECT_FALSE(authenticator->authenticate("wrong-password").has_value());
}

// The fixtures above cover R 2 (RC4-40) and R 6 (AES-256). The remaining R 3
// (RC4-128) and R 4 (AES-128 / AESV2) paths have no real-world fixture, so the
// vectors below were produced by `qpdf --encrypt user owner 128 ...` over a
// one-object content stream holding the marker "KAT-MARKER-12345". Decrypting
// qpdf's output back to the marker proves the whole chain — file key,
// per-object key (Algorithm 1, incl. the AES "sAlT"), RC4 / AES-128-CBC +
// PKCS#7 — against an independent implementation, with no fixture file to ship.
namespace {

constexpr const char *kMarker = "KAT-MARKER-12345";
const ObjectReference kContentRef{4, 0};

Dictionary standard_encrypt(const Integer v, const Integer r,
                            const Integer length_bits, const std::string &o,
                            const std::string &u) {
  Dictionary e;
  e["Filter"] = Object(Name("Standard"));
  e["V"] = Object(v);
  e["R"] = Object(r);
  e["Length"] = Object(length_bits);
  e["P"] = Object(Integer(-4));
  e["O"] = Object(StandardString(o));
  e["U"] = Object(StandardString(u));
  return e;
}

// V 4 with a single AESV2 StdCF crypt filter for both streams and strings.
Dictionary aesv2_encrypt(const std::string &o, const std::string &u,
                         bool encrypt_metadata) {
  Dictionary e = standard_encrypt(4, 4, 128, o, u);
  Dictionary std_cf;
  std_cf["CFM"] = Object(Name("AESV2"));
  Dictionary cf;
  cf["StdCF"] = Object(std::move(std_cf));
  e["CF"] = Object(std::move(cf));
  e["StmF"] = Object(Name("StdCF"));
  e["StrF"] = Object(Name("StdCF"));
  if (!encrypt_metadata) {
    e["EncryptMetadata"] = Object(false);
  }
  return e;
}

} // namespace

TEST(PdfEncryption, qpdf_rc4_128_r3) {
  const Dictionary encrypt = standard_encrypt(
      2, 3, 128,
      hex_decode(
          "0ba3835f88f90388e74e54584125ce142be0de24c6b0d37746e075b891756671"),
      hex_decode(
          "cfc31bd6a458aadef418ac4b427dfe010021446990b9e4114071a4d9104984c1"));

  const auto authenticator = Authenticator::create(
      encrypt, hex_decode("5aff498eede5e840196289c653e0eb44"));
  ASSERT_TRUE(authenticator.has_value());
  const auto decryptor = authenticator->authenticate("user");
  ASSERT_TRUE(decryptor.has_value());
  const std::string content = decryptor->decrypt_stream(
      kContentRef,
      hex_decode(
          "aedffca5d3c5f617143550c4ef045a59853eb36b790c4a08e24b104ed1eb55e1"
          "31bccd2aa62905dfa1dcd6cd56c1a46c"));
  EXPECT_NE(content.find(kMarker), std::string::npos);
}

TEST(PdfEncryption, qpdf_aes_128_r4) {
  const Dictionary encrypt = aesv2_encrypt(
      hex_decode(
          "0ba3835f88f90388e74e54584125ce142be0de24c6b0d37746e075b891756671"),
      hex_decode(
          "be561e5a355dee484698c6050e4d47680021446990b9e4114071a4d9104984c1"),
      true);

  const auto authenticator = Authenticator::create(
      encrypt, hex_decode("e0ab9dc9926ecd79c7ea59739043f1fa"));
  ASSERT_TRUE(authenticator.has_value());
  const auto decryptor = authenticator->authenticate("user");
  ASSERT_TRUE(decryptor.has_value());
  const std::string content = decryptor->decrypt_stream(
      kContentRef,
      hex_decode(
          "1cdcf005888596d73142dfd077adce936261e5c7a369cfd8301ea2af46161262"
          "08ece51b9e09e3559b0c3391baa43e00cfa9fd74eacb83ea0659d3543fc29040"
          "0520fe64db7c4afc4ba9c9762a7c793c"));
  EXPECT_NE(content.find(kMarker), std::string::npos);
}

// EncryptMetadata false adds the 0xFFFFFFFF salt to the key derivation
// (Algorithm 2, step f).
TEST(PdfEncryption, qpdf_aes_128_r4_cleartext_metadata) {
  const Dictionary encrypt = aesv2_encrypt(
      hex_decode(
          "0ba3835f88f90388e74e54584125ce142be0de24c6b0d37746e075b891756671"),
      hex_decode(
          "08deff331b7a9a4643ae51064bcfd6510021446990b9e4114071a4d9104984c1"),
      false);

  const auto authenticator = Authenticator::create(
      encrypt, hex_decode("1444f0ada1f2311db55de9a7cb4ded37"));
  ASSERT_TRUE(authenticator.has_value());
  const auto decryptor = authenticator->authenticate("user");
  ASSERT_TRUE(decryptor.has_value());
  const std::string content = decryptor->decrypt_stream(
      kContentRef,
      hex_decode(
          "95ad3265670d8686bfe7913688cb4ef23a0b864db3192bb2584d58882d5aff85"
          "cf0f1663e136e96669c6ef3ab2a1848dc16e212d068a8501a13ed831323d894f"
          "056d6a0bfab626eca402b3f3d2b44513"));
  EXPECT_NE(content.find(kMarker), std::string::npos);
}

// Owner-locked file (empty user password): the empty user password opens it,
// and the owner password recovers the same key via Algorithm 7.
TEST(PdfEncryption, qpdf_aes_128_r4_owner_password) {
  const std::string o = hex_decode(
      "566fa873ee33c797cd3b904fdadf814afa34df9a38f6ed41b984e2c6da2aa6f5");
  const std::string u = hex_decode(
      "5148a4f9990604380199d16040dc7dc30021446990b9e4114071a4d9104984c1");
  const std::string id0 = hex_decode("db3ccbe95b12f1337444fa5c35e345cf");
  const std::string stream = hex_decode(
      "3250f1387c26e539cfbeb2662e0d6a203cc530bdbb9ae1d6c8edb8aa2a4c4c5d"
      "a7ed75f1ea892c0a938d47a831920948d911c53157874e5b6a3568589353ad03"
      "4ac8982fbe8385589f5175e5cbec5148");

  const auto authenticator =
      Authenticator::create(aesv2_encrypt(o, u, true), id0);
  ASSERT_TRUE(authenticator.has_value());

  const auto decryptor_user = authenticator->authenticate("");
  ASSERT_TRUE(decryptor_user.has_value());
  EXPECT_NE(decryptor_user->decrypt_stream(kContentRef, stream).find(kMarker),
            std::string::npos);

  const auto decryptor_owner = authenticator->authenticate("owner");
  ASSERT_TRUE(decryptor_owner.has_value());
  EXPECT_NE(decryptor_owner->decrypt_stream(kContentRef, stream).find(kMarker),
            std::string::npos);
}
