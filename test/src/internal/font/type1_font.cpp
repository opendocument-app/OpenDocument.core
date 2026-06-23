#include <odr/internal/font/type1_font.hpp>

#include <odr/internal/font/cff_font.hpp>
#include <odr/internal/font/cff_transform.hpp>
#include <odr/internal/font/sfnt_font.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>

using namespace odr::internal::font::type1;

namespace {

/// Forward Type1 cipher (the inverse of `decrypt`), so the test builds a real
/// encrypted program rather than trusting the decryptor.
std::string encrypt(const std::string &plain, std::uint16_t r,
                    const std::string &random_prefix) {
  constexpr std::uint16_t c1 = 52845;
  constexpr std::uint16_t c2 = 22719;
  std::string out;
  for (const char ch : random_prefix + plain) {
    const auto p = static_cast<std::uint8_t>(ch);
    const auto cipher = static_cast<std::uint8_t>(p ^ (r >> 8));
    out += static_cast<char>(cipher);
    r = static_cast<std::uint16_t>((cipher + r) * c1 + c2);
  }
  return out;
}

/// A `/name len RD <bytes> ND` charstring entry, the charstring encrypted with
/// the charstring key (4330) and a 4-byte lenIV prefix.
std::string charstring_entry(const std::string &name,
                             const std::string &plain_charstring) {
  // The 4-byte lenIV prefix must be a real 4 NUL bytes — a "\x00\x00\x00\x00"
  // string literal would be empty (the first NUL terminates it).
  const std::string enc = encrypt(plain_charstring, 4330, std::string(4, '\0'));
  return "/" + name + " " + std::to_string(enc.size()) + " RD " + enc + " ND\n";
}

/// Assemble a minimal but well-formed Type1 program: a clear header (with a
/// custom /Encoding and /FontMatrix) and an eexec-encrypted private section
/// holding two glyphs and one subr.
std::string build_type1() {
  std::string clear = "%!PS-AdobeFont-1.0: TestType1 001.000\n"
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
                                "/lenIV 4 def\n"
                                "/Subrs 1 array\n";
  private_section += "dup 0 ";
  {
    const std::string subr =
        encrypt(std::string("\x0b", 1), 4330, std::string(4, '\0')); // return
    private_section += std::to_string(subr.size()) + " RD " + subr + " NP\n";
  }
  private_section += "ND\n"
                     "2 index /CharStrings 2 dict dup begin\n";
  // .notdef-ish + two named glyphs. Charstring bytes are arbitrary here: the
  // parser does not interpret them, it only extracts them.
  private_section += charstring_entry("A", std::string("\x8b\x8b\x0d\x0e", 4));
  private_section += charstring_entry("B", std::string("\xf0\x0d\x0e", 3));
  private_section += "end\nend\n";

  std::string program = clear;
  program += encrypt(private_section, 55665, "wxyz");
  // Trailer (would be 512 zeros + cleartomark in a real font); the parser
  // tolerates trailing data, so a short stub is enough.
  program += std::string(8, '\0');
  return program;
}

} // namespace

TEST(Type1FontTest, IsType1Magic) {
  EXPECT_TRUE(Type1Program::is_type1(build_type1()));
  EXPECT_FALSE(Type1Program::is_type1("not a font program at all"));
}

TEST(Type1FontTest, ParsesHeaderAndEncoding) {
  const Type1Program font{build_type1()};

  EXPECT_EQ(font.name(), "TestType1");
  EXPECT_FALSE(font.standard_encoding());
  ASSERT_EQ(font.font_matrix().size(), 6u);
  EXPECT_DOUBLE_EQ(font.font_matrix()[0], 0.001);
  EXPECT_EQ(font.font_bbox().y_min, -200);
  EXPECT_EQ(font.font_bbox().x_max, 700);

  EXPECT_EQ(font.encoding().at(65), "A");
  EXPECT_EQ(font.encoding().at(66), "B");
}

TEST(Type1FontTest, DecryptsCharstringsAndSubrs) {
  const Type1Program font{build_type1()};

  ASSERT_EQ(font.glyphs().size(), 2u);
  EXPECT_EQ(font.glyphs()[0].name, "A");
  EXPECT_EQ(font.glyphs()[0].charstring, std::string("\x8b\x8b\x0d\x0e", 4));
  EXPECT_EQ(font.glyphs()[1].name, "B");
  EXPECT_EQ(font.glyphs()[1].charstring, std::string("\xf0\x0d\x0e", 3));

  ASSERT_EQ(font.subrs().size(), 1u);
  EXPECT_EQ(font.subrs()[0], std::string("\x0b", 1)); // return
}

TEST(Type1FontTest, ConvertsToLoadableCff) {
  namespace cff = odr::internal::font::cff;
  namespace sfnt = odr::internal::font::sfnt;

  const Type1Program program{build_type1()};
  const std::string cff_bytes = to_cff(program);

  const cff::CffFont font{cff_bytes};
  EXPECT_EQ(font.format(), odr::FontFormat::cff);
  // .notdef (synthesized, since the test font has none) + A + B.
  EXPECT_EQ(font.glyph_count(), 3);
  EXPECT_EQ(font.glyph_name(1), "A");
  EXPECT_EQ(font.glyph_name(2), "B");

  // The converted CFF wraps into a browser-loadable OTTO (the 3.4 path).
  EXPECT_TRUE(sfnt::SfntFont::is_sfnt(cff::wrap_to_otf(font)));
}
