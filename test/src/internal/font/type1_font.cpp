#include <odr/internal/font/type1_font.hpp>
#include <odr/internal/font/type1_transform.hpp>

#include <odr/internal/font/cff_font.hpp>
#include <odr/internal/font/cff_transform.hpp>
#include <odr/internal/font/sfnt_font.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <internal/font/type1_test_util.hpp>
#include <locale_util.hpp>

#include <gtest/gtest.h>

#include <clocale>
#include <cstdint>
#include <string>

using namespace odr::internal::font::type1;

namespace {

using odr::test::font::encrypt;

/// A charstring entry encoded according to /lenIV.
std::string charstring_entry(const std::string &name,
                             const std::string &plain_charstring,
                             const std::int32_t len_iv) {
  const std::string enc =
      len_iv == -1
          ? plain_charstring
          : encrypt(plain_charstring, 4330,
                    std::string(static_cast<std::size_t>(len_iv), '\0'));
  return "/" + name + " " + std::to_string(enc.size()) + " RD " + enc + " ND\n";
}

/// Type1 fixture with two glyphs and one subroutine.
std::string
build_type1(const std::int32_t len_iv = 4, const std::string &prefix = "wxyz",
            const std::string &glyph_b = std::string("\xf0\x0d\x0e", 3),
            const std::string &name_b = "B") {
  const std::string clear = "%!PS-AdobeFont-1.0: TestType1 001.000\n"
                            "/FontName /TestType1 def\n"
                            "/FontMatrix [0.001 0 0 0.001 0 0] readonly def\n"
                            "/FontBBox {0 -200 700 800} readonly def\n"
                            "/Encoding 256 array\n"
                            "0 1 255 {1 index exch /.notdef put} for\n"
                            "dup 65 /A put\n"
                            "dup 66 /B put\n"
                            "readonly def\n"
                            "currentdict end\n"
                            "currentfile eexec\n";

  std::string private_section = "dup /Private 16 dict dup begin\n"
                                "/lenIV " +
                                std::to_string(len_iv) +
                                " def\n/Subrs 1 array\n";
  private_section += "dup 0 ";
  {
    const std::string plain("\x0b", 1);
    const std::string subr =
        len_iv == -1
            ? plain
            : encrypt(plain, 4330,
                      std::string(static_cast<std::size_t>(len_iv), '\0'));
    private_section += std::to_string(subr.size()) + " RD " + subr + " NP\n";
  }
  private_section += "ND\n"
                     "2 index /CharStrings 2 dict dup begin\n";
  // .notdef-ish + two named glyphs. Charstring bytes are arbitrary here: the
  // parser does not interpret them, it only extracts them.
  private_section +=
      charstring_entry("A", std::string("\x8b\x8b\x0d\x0e", 4), len_iv);
  private_section += charstring_entry(name_b, glyph_b, len_iv);
  private_section += "end\nend\n";

  std::string program = clear;
  program += encrypt(private_section, 55665, prefix);
  // Trailer (would be 512 zeros + cleartomark in a real font); the parser
  // tolerates trailing data, so a short stub is enough.
  program += std::string(8, '\0');
  return program;
}

} // namespace

TEST(Type1FontTest, IsType1Magic) {
  EXPECT_TRUE(Type1Font::is_type1(build_type1()));
  EXPECT_FALSE(Type1Font::is_type1("not a font program at all"));
}

TEST(Type1FontTest, ParsesHeaderAndEncoding) {
  const odr::test::LocaleGuard guard;
  if (std::setlocale(LC_NUMERIC, "de_DE.UTF-8") == nullptr) {
    std::setlocale(LC_NUMERIC, "de_DE.utf8");
  }
  const Type1Font font{build_type1()};

  EXPECT_EQ(font.name(), "TestType1");
  EXPECT_FALSE(font.standard_encoding());
  EXPECT_DOUBLE_EQ(font.font_matrix().a, 0.001);
  EXPECT_DOUBLE_EQ(font.font_matrix().d, 0.001);
  EXPECT_EQ(font.font_bbox().y_min, -200);
  EXPECT_EQ(font.font_bbox().x_max, 700);

  EXPECT_EQ(font.encoding().at(65), "A");
  EXPECT_EQ(font.encoding().at(66), "B");
}

TEST(Type1FontTest, DecryptsCharstringsAndSubrs) {
  for (const std::int32_t len_iv : {-1, 0, 4}) {
    const Type1Font font{build_type1(len_iv, std::string("\xd9xyz", 4))};

    ASSERT_EQ(font.glyphs().size(), 2u);
    EXPECT_EQ(font.glyphs()[0].name, "A");
    EXPECT_EQ(font.glyphs()[0].charstring, std::string("\x8b\x8b\x0d\x0e", 4));
    EXPECT_EQ(font.glyphs()[1].name, "B");
    EXPECT_EQ(font.glyphs()[1].charstring, std::string("\xf0\x0d\x0e", 3));

    ASSERT_EQ(font.subrs().size(), 1u);
    EXPECT_EQ(font.subrs()[0], std::string("\x0b", 1)); // return
  }
}

TEST(Type1FontTest, ConvertsToLoadableCff) {
  namespace cff = odr::internal::font::cff;
  namespace sfnt = odr::internal::font::sfnt;

  const Type1Font type1_font{build_type1()};
  const std::string cff_bytes = to_cff(type1_font);

  const cff::CffFont font{cff_bytes};
  EXPECT_EQ(font.format(), odr::FontFormat::cff);
  // .notdef (synthesized, since the test font has none) + A + B.
  EXPECT_EQ(font.glyph_count(), 3);
  EXPECT_EQ(font.glyph_name(1), "A");
  EXPECT_EQ(font.glyph_name(2), "B");

  // The converted CFF wraps into a browser-loadable OTTO (the 3.4 path).
  EXPECT_TRUE(sfnt::SfntFont::is_sfnt(cff::wrap_to_otf(font)));
}

TEST(Type1FontTest, BrokenGlyphBecomesEmptyInCff) {
  // 1 0 div: the translation throws on the division by zero.
  const Type1Font font{
      build_type1(4, "wxyz", std::string("\x8c\x8b\x0c\x0c\x0e", 5))};
  const odr::internal::font::cff::CffFont cff(to_cff(font));
  EXPECT_EQ(cff.glyph_count(), 3);
  EXPECT_EQ(cff.glyph_name(2), "B");
}

TEST(Type1FontTest, RepeatedGlyphNameKeepsTheFirstInCff) {
  const std::string program =
      build_type1(4, "wxyz", std::string("\xf0\x0d\x0e", 3), "A");
  const odr::internal::font::cff::CffFont cff(to_cff(Type1Font{program}));
  EXPECT_EQ(cff.glyph_count(), 2);
  EXPECT_EQ(cff.glyph_name(1), "A");
}

TEST(Type1FontTest, ReadsInvalidNumbersAsZeroAndRejectsBadPfbSegments) {
  for (const std::string_view number : {"nan", "inf", "1oops"}) {
    std::string program = build_type1();
    program.replace(program.find("0.001"), 5, number);
    EXPECT_EQ(Type1Font{program}.glyphs().size(), 2);
  }
  const std::string program = build_type1();
  std::string pfb("\x80\x01", 2);
  odr::internal::util::byte_string::put_u32_le(
      pfb, static_cast<std::uint32_t>(program.size()));
  pfb += program;
  EXPECT_EQ(Type1Font(pfb + std::string("\x80\x03", 2)).glyphs().size(), 2);
  EXPECT_THROW(Type1Font(pfb + std::string("\x80\x02\xff\xff\xff\xff", 6)),
               std::runtime_error);
  pfb[1] = 4;
  EXPECT_THROW(Type1Font{pfb}, std::runtime_error);
}
