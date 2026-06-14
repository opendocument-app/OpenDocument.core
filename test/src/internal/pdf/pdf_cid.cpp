#include <odr/internal/pdf/pdf_cid.hpp>

#include <string>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

// The `Uni*-UCS2/UTF16-H` CMaps carry UTF-16BE codes directly.
TEST(PdfCid, unicode_cmap_ucs2) {
  // U+0041 'A' and U+4E2D '中' (UTF-8 e4 b8 ad).
  const std::string codes("\x00\x41\x4e\x2d", 4);
  const auto unicode = translate_predefined_cmap("UniGB-UCS2-H", codes);
  ASSERT_TRUE(unicode.has_value());
  EXPECT_EQ(*unicode, "A\xe4\xb8\xad");
}

// UTF-16 surrogate pairs decode to their astral code point.
TEST(PdfCid, unicode_cmap_utf16_surrogate_pair) {
  // U+1F600 '😀' = surrogate pair D83D DE00 (UTF-8 f0 9f 98 80).
  const std::string codes("\xd8\x3d\xde\x00", 4);
  const auto unicode = translate_predefined_cmap("UniJIS-UTF16-H", codes);
  ASSERT_TRUE(unicode.has_value());
  EXPECT_EQ(*unicode, "\xf0\x9f\x98\x80");
}

// The `Uni*-UTF32-H` CMaps carry 4-byte big-endian code points.
TEST(PdfCid, unicode_cmap_utf32) {
  const std::string codes("\x00\x01\xf6\x00", 4); // U+1F600
  const auto unicode = translate_predefined_cmap("UniCNS-UTF32-H", codes);
  ASSERT_TRUE(unicode.has_value());
  EXPECT_EQ(*unicode, "\xf0\x9f\x98\x80");
}

// Vertical writing-mode variants decode identically.
TEST(PdfCid, unicode_cmap_vertical_variant) {
  const std::string codes("\x00\x41", 2);
  EXPECT_EQ(translate_predefined_cmap("UniKS-UCS2-V", codes), "A");
}

// `Identity-H/V` and the legacy CJK→CID CMaps are not Unicode CMaps: no value
// (the caller then yields "no Unicode").
TEST(PdfCid, non_unicode_cmaps_return_nullopt) {
  const std::string codes("\x00\x41", 2);
  EXPECT_FALSE(translate_predefined_cmap("Identity-H", codes).has_value());
  EXPECT_FALSE(translate_predefined_cmap("90ms-RKSJ-H", codes).has_value());
  EXPECT_FALSE(translate_predefined_cmap("GBK-EUC-H", codes).has_value());
}
