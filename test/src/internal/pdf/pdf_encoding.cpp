#include <odr/internal/pdf/pdf_encoding.hpp>

#include <string>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

TEST(PdfEncoding, base_encoding_from_name) {
  EXPECT_EQ(base_encoding_from_name("WinAnsiEncoding"), BaseEncoding::win_ansi);
  EXPECT_EQ(base_encoding_from_name("StandardEncoding"),
            BaseEncoding::standard);
  EXPECT_EQ(base_encoding_from_name("MacRomanEncoding"),
            BaseEncoding::mac_roman);
  EXPECT_FALSE(base_encoding_from_name("MacExpertEncoding").has_value());
}

TEST(PdfEncoding, glyph_name_to_unicode_agl) {
  EXPECT_EQ(glyph_name_to_unicode("A"), u"A");
  EXPECT_EQ(glyph_name_to_unicode("space"), u" ");
  // Ligature glyph name -> multi-character target.
  EXPECT_EQ(glyph_name_to_unicode("fi"), u"fi");
  // A composite name uses the part before the first '.'.
  EXPECT_EQ(glyph_name_to_unicode("A.sc"), u"A");
  // Unknown name -> "no Unicode".
  EXPECT_EQ(glyph_name_to_unicode("notaglyphname"), u"");
}

TEST(PdfEncoding, glyph_name_to_unicode_algorithmic) {
  EXPECT_EQ(glyph_name_to_unicode("uni0041"), u"A");
  EXPECT_EQ(glyph_name_to_unicode("uni00410042"), u"AB");
  EXPECT_EQ(glyph_name_to_unicode("u0041"), u"A");
  EXPECT_EQ(glyph_name_to_unicode("u1F600"), u"\U0001F600");
  // Malformed forms map to nothing.
  EXPECT_EQ(glyph_name_to_unicode("uni004"), u"");
  EXPECT_EQ(glyph_name_to_unicode("uniZZZZ"), u"");
}

TEST(PdfEncoding, translate_string_base) {
  Encoding encoding(BaseEncoding::win_ansi);
  EXPECT_EQ(encoding.translate_string("ABC"), "ABC");
}

TEST(PdfEncoding, translate_string_differences) {
  Encoding encoding(BaseEncoding::win_ansi);
  // Remap code 0x41 ('A') to the glyph "bullet".
  encoding.set_difference(0x41, "bullet");
  EXPECT_EQ(encoding.glyph_name(0x41), "bullet");
  EXPECT_EQ(encoding.translate_string("A"), "•");
  // Untouched codes keep the base mapping.
  EXPECT_EQ(encoding.translate_string("B"), "B");
}

TEST(PdfEncoding, win_ansi_vs_standard_quote) {
  // Code 0x27 differs: WinAnsi -> quotesingle (U+0027), Standard -> quoteright
  // (U+2019).
  EXPECT_EQ(Encoding(BaseEncoding::win_ansi).translate_string("\x27"), "'");
  EXPECT_EQ(Encoding(BaseEncoding::standard).translate_string("\x27"), "’");
}
