#include <odr/internal/pdf/pdf_encryption.hpp>
#include <odr/internal/pdf/pdf_object.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace odr::internal::pdf;

namespace {
// Decode a hex literal into the raw bytes a parsed PDF string would hold.
std::string unhex(const std::string &hex) {
  const auto nibble = [](char c) -> int {
    if (c >= '0' && c <= '9') {
      return c - '0';
    }
    return (c | 0x20) - 'a' + 10;
  };
  std::string out;
  for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
    out.push_back(static_cast<char>(nibble(hex[i]) * 16 + nibble(hex[i + 1])));
  }
  return out;
}
} // namespace

// Known-answer test from the real R 2 / RC4-40 fixture
// (odr-public/pdf/Casio_WVA-M650-7AJF.pdf): derive the file key from /O, /P and
// /ID[0] with the empty user password, then Algorithm 4 must reproduce /U. The
// vectors come from a real producer, so this is not circular through our code.
TEST(PdfEncryption, casio_r2_rc4_user_key) {
  const std::string o =
      unhex("5ee983cbdfb3f64baa265a496c450467891a307fd45c5f7d53282fd1835ce1b1");
  const std::string u =
      unhex("d3f7b55d8d09b0efb6b4c1cc158ea7eaa579f693d93aeccf2d87e99b78376a18");
  const std::string id0 = unhex("f2bffd9443e6910523d547b66f58e0c8");

  const std::string key = standard_security::compute_key_r2_r4(
      /*password=*/"", o, /*p=*/-60, id0, /*r=*/2, /*key_length=*/5,
      /*encrypt_metadata=*/true);
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
  encrypt["O"] = Object(StandardString(unhex(
      "5ee983cbdfb3f64baa265a496c450467891a307fd45c5f7d53282fd1835ce1b1")));
  encrypt["U"] = Object(StandardString(unhex(
      "d3f7b55d8d09b0efb6b4c1cc158ea7eaa579f693d93aeccf2d87e99b78376a18")));

  auto decryptor =
      Decryptor::create(encrypt, unhex("f2bffd9443e6910523d547b66f58e0c8"));
  ASSERT_TRUE(decryptor.has_value());
  EXPECT_TRUE(decryptor->authenticate(""));
  EXPECT_TRUE(decryptor->authenticated());
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
  encrypt["O"] = Object(StandardString(
      unhex("d2549e6679b224b4d049a48571f5293451a389a88358c8f5311d3d11ae001154"
            "5da942eca4591a7eb8d4d5619a750129")));
  encrypt["U"] = Object(StandardString(
      unhex("3691dd81a3d444ad86131ab545c0251d7a352e95c4018810f554259a5a3b2332"
            "3633a7fd430e76322636e7739e4b97c1")));
  encrypt["OE"] = Object(StandardString(unhex(
      "98ed9b3c3e14b6456992859d7c752c015479e7c6beb22e8b1f270d2e77e436d3")));
  encrypt["UE"] = Object(StandardString(unhex(
      "973fa8deba0ffd1cf8a58783d15532316097831d51ea722465283873c7bf2954")));
  const std::string id0 = unhex("48f4ed84fa38cfb32c9641b2442e6a7d");

  auto decryptor = Decryptor::create(encrypt, id0);
  ASSERT_TRUE(decryptor.has_value());
  EXPECT_TRUE(decryptor->authenticate("sample-user-password"));
  EXPECT_TRUE(decryptor->authenticated());

  auto wrong = Decryptor::create(encrypt, id0);
  EXPECT_FALSE(wrong->authenticate("wrong-password"));
  EXPECT_FALSE(wrong->authenticated());
}
