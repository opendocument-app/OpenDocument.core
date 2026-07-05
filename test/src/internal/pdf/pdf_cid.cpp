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

// A legacy Shift-JIS CMap (Adobe-Japan1): two-byte codes split, map to CIDs,
// then to Unicode through the collection table.
TEST(PdfCid, legacy_cmap_shift_jis) {
  // \x92\x86 -> U+4E2D '中', \x82\xa0 -> U+3042 'あ'.
  const std::string codes("\x92\x86\x82\xa0", 4);
  EXPECT_EQ(translate_predefined_cmap("90ms-RKSJ-H", codes),
            "\xe4\xb8\xad\xe3\x81\x82");
}

// A mixed-width codespace: a single-byte code among two-byte ones stays
// aligned.
TEST(PdfCid, legacy_cmap_mixed_width) {
  // \xa1 (1-byte) -> U+FF61 '｡', then \x92\x86 (2-byte) -> U+4E2D '中'.
  const std::string codes("\xa1\x92\x86", 3);
  EXPECT_EQ(translate_predefined_cmap("90ms-RKSJ-H", codes),
            "\xef\xbd\xa1\xe4\xb8\xad");
}

// A legacy EUC CMap over a different collection (Adobe-GB1).
TEST(PdfCid, legacy_cmap_euc_gb) {
  // \xd6\xd0 -> U+4E2D '中', \xb9\xfa -> U+56FD '国'.
  const std::string codes("\xd6\xd0\xb9\xfa", 4);
  EXPECT_EQ(translate_predefined_cmap("GBK-EUC-H", codes),
            "\xe4\xb8\xad\xe5\x9b\xbd");
}

// GB18030 (GBK2K) overlaps lead bytes: a 2-byte range and a 4-byte range both
// start 0x81-0xFE, so the code width must be decided past the first byte.
TEST(PdfCid, legacy_cmap_gb18030_four_byte) {
  // \x81\x30\x84\x36 is a 4-byte code -> CID 22354 -> U+00A5 '¥' (UTF-8 c2 a5).
  const std::string codes("\x81\x30\x84\x36", 4);
  EXPECT_EQ(translate_predefined_cmap("GBK2K-H", codes), "\xc2\xa5");
}

// `Identity-H/V` is not a predefined table CMap (code == CID; the collection,
// hence CID -> Unicode, comes from `/CIDSystemInfo` via `cid_to_unicode`), and
// an unshipped CMap name is unknown.
TEST(PdfCid, unresolved_cmaps_return_nullopt) {
  const std::string codes("\x00\x41", 2);
  EXPECT_FALSE(translate_predefined_cmap("Identity-H", codes).has_value());
  EXPECT_FALSE(translate_predefined_cmap("not-a-cmap", codes).has_value());
}

// CID -> Unicode for a collection named by /CIDSystemInfo (the Identity-H
// path).
TEST(PdfCid, cid_to_unicode_by_collection) {
  EXPECT_EQ(cid_to_unicode("Adobe", "Japan1", 2980), U'\x4E2D'); // 中
  EXPECT_EQ(cid_to_unicode("Adobe", "GB1", 4559), U'\x4E2D');    // 中
  // Astral CID -> Unicode escapes through the side table.
  EXPECT_EQ(cid_to_unicode("Adobe", "Japan1", 7641), U'\U00028CDD');
  // Unknown collection (the Identity ordering carries no Unicode) and an
  // out-of-range CID both yield nullopt.
  EXPECT_FALSE(cid_to_unicode("Adobe", "Identity", 5).has_value());
  EXPECT_FALSE(cid_to_unicode("Adobe", "Japan1", 999999).has_value());
}
