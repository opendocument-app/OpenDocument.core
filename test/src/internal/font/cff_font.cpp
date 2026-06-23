#include <odr/internal/font/cff_font.hpp>

#include <odr/font.hpp>
#include <odr/internal/font/cff_transform.hpp>
#include <odr/internal/font/sfnt_font.hpp>
#include <odr/internal/font/sfnt_transform.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::internal::font::cff;

namespace {

void put16(std::string &s, const std::uint16_t v) {
  s += static_cast<char>(v >> 8);
  s += static_cast<char>(v & 0xff);
}

/// A DICT integer, always in the 5-byte `29 + int32` form so the Top DICT has a
/// fixed size independent of the (offset) values — lets the builder lay the
/// font out in a single pass.
void dict_int(std::string &s, const std::int32_t v) {
  s += static_cast<char>(29);
  s += static_cast<char>((v >> 24) & 0xff);
  s += static_cast<char>((v >> 16) & 0xff);
  s += static_cast<char>((v >> 8) & 0xff);
  s += static_cast<char>(v & 0xff);
}

/// Serialize a CFF INDEX from its members.
std::string build_index(const std::vector<std::string> &members) {
  std::string out;
  put16(out, static_cast<std::uint16_t>(members.size()));
  if (members.empty()) {
    return out; // count 0: no offSize/offsets
  }
  std::uint32_t total = 1;
  for (const std::string &m : members) {
    total += static_cast<std::uint32_t>(m.size());
  }
  const std::uint8_t off_size = total <= 0xff ? 1 : total <= 0xffff ? 2 : 4;
  out += static_cast<char>(off_size);
  const auto put_off = [&](std::uint32_t off) {
    for (int i = off_size - 1; i >= 0; --i) {
      out += static_cast<char>((off >> (8 * i)) & 0xff);
    }
  };
  std::uint32_t offset = 1;
  put_off(offset);
  for (const std::string &m : members) {
    offset += static_cast<std::uint32_t>(m.size());
    put_off(offset);
  }
  for (const std::string &m : members) {
    out += m;
  }
  return out;
}

/// Build a minimal name-keyed CFF: two glyphs (.notdef + one named glyph). When
/// @p glyph1_sid is a custom SID (>= 391) the name comes from the String INDEX
/// ("myglyph"); a standard SID (< 391) names the glyph via the CFF standard
/// strings (no String INDEX entry needed).
std::string build_cff(const std::uint16_t glyph1_sid = 391) {
  // Charstrings (Type2): glyph 0 = endchar; glyph 1 = width-operand 50,
  // endchar. operand 50 -> single byte 50 + 139 = 189; endchar = 14.
  const std::string cs_notdef(1, static_cast<char>(14));
  std::string cs_glyph;
  cs_glyph += static_cast<char>(50 + 139);
  cs_glyph += static_cast<char>(14);
  const std::string charstrings = build_index({cs_notdef, cs_glyph});

  // charset format 0: glyph 1 -> the requested SID.
  std::string charset;
  charset += static_cast<char>(0); // format 0
  put16(charset, glyph1_sid);

  // Private DICT: defaultWidthX 500 (op 20), nominalWidthX 200 (op 21).
  std::string private_dict;
  dict_int(private_dict, 500);
  private_dict += static_cast<char>(20);
  dict_int(private_dict, 200);
  private_dict += static_cast<char>(21);

  const std::string name_index = build_index({"TestFont"});
  const std::string string_index =
      glyph1_sid >= 391 ? build_index({"myglyph"}) : build_index({});
  const std::string global_subrs = build_index({});

  // Top DICT body with placeholder offsets to measure its (fixed) size, then
  // again with the real absolute offsets once the layout is known.
  const auto top_dict = [&](std::uint32_t cs_off, std::uint32_t charset_off,
                            std::uint32_t priv_off) {
    std::string d;
    dict_int(d, 0);
    dict_int(d, -200);
    dict_int(d, 600);
    dict_int(d, 800);
    d += static_cast<char>(5); // FontBBox
    dict_int(d, static_cast<std::int32_t>(charset_off));
    d += static_cast<char>(15); // charset
    dict_int(d, static_cast<std::int32_t>(cs_off));
    d += static_cast<char>(17); // CharStrings
    dict_int(d, static_cast<std::int32_t>(private_dict.size()));
    dict_int(d, static_cast<std::int32_t>(priv_off));
    d += static_cast<char>(18); // Private [size offset]
    return d;
  };

  const std::string top_dict_index_probe = build_index({top_dict(0, 0, 0)});
  const std::uint32_t header_size = 4;
  const std::uint32_t prefix =
      header_size + static_cast<std::uint32_t>(name_index.size()) +
      static_cast<std::uint32_t>(top_dict_index_probe.size()) +
      static_cast<std::uint32_t>(string_index.size()) +
      static_cast<std::uint32_t>(global_subrs.size());
  const std::uint32_t cs_off = prefix;
  const std::uint32_t charset_off =
      cs_off + static_cast<std::uint32_t>(charstrings.size());
  const std::uint32_t priv_off =
      charset_off + static_cast<std::uint32_t>(charset.size());

  const std::string top_dict_index =
      build_index({top_dict(cs_off, charset_off, priv_off)});

  std::string out;
  out += static_cast<char>(1); // major
  out += static_cast<char>(0); // minor
  out += static_cast<char>(4); // hdrSize
  out += static_cast<char>(1); // offSize
  out += name_index;
  out += top_dict_index;
  out += string_index;
  out += global_subrs;
  out += charstrings;
  out += charset;
  out += private_dict;
  return out;
}

} // namespace

TEST(CffFontTest, ParsesFactsFromMinimalFont) {
  const CffFont font{build_cff()};

  EXPECT_EQ(font.format(), FontFormat::cff);
  EXPECT_EQ(font.name(), "TestFont");
  EXPECT_EQ(font.glyph_count(), 2);
  EXPECT_EQ(font.units_per_em(), 1000); // no FontMatrix -> default
  EXPECT_FALSE(font.is_cid_keyed());

  const FontBBox bbox = font.bounding_box();
  EXPECT_EQ(bbox.x_min, 0);
  EXPECT_EQ(bbox.y_min, -200);
  EXPECT_EQ(bbox.x_max, 600);
  EXPECT_EQ(bbox.y_max, 800);
}

TEST(CffFontTest, ResolvesCustomGlyphNameAndWidth) {
  const CffFont font{build_cff()};

  // glyph 1 maps to the first String INDEX entry (SID 391).
  EXPECT_EQ(font.glyph_name(1), "myglyph");
  // explicit charstring width: nominalWidthX (200) + operand (50).
  EXPECT_EQ(font.advance_width(1), 250);
  // glyph 0 has no explicit width: defaultWidthX (500).
  EXPECT_EQ(font.advance_width(0), 500);
}

TEST(CffFontTest, ResolvesStandardStringAndReverseMap) {
  // CFF standard string SID 34 is "A"; the glyph -> name -> Unicode reverse map
  // goes through the AGL (pdf module).
  const CffFont font{build_cff(/*glyph1_sid=*/34)};

  EXPECT_EQ(font.glyph_name(1), "A");
  // reverse map: glyph 1 ("A") -> U+0041 via the AGL.
  ASSERT_TRUE(font.code_point_for_glyph(1).has_value());
  EXPECT_EQ(font.code_point_for_glyph(1), U'A');
  // forward: U+0041 -> glyph 1.
  EXPECT_EQ(font.glyph_for_code_point(U'A'), 1);
}

TEST(CffFontTest, IsCffMagic) {
  EXPECT_TRUE(CffFont::is_cff(build_cff()));
  EXPECT_FALSE(CffFont::is_cff(std::string("\x00\x01\x00\x00", 4)));
  EXPECT_FALSE(CffFont::is_cff("not a font"));
}

TEST(CffFontTest, WrapsToLoadableOtf) {
  using namespace odr::internal::font;
  const CffFont cff{build_cff()};
  const std::string otf = cff::wrap_to_otf(cff);

  // The wrap is a valid OTTO that parses back as an SFNT carrying the CFF as a
  // pass-through table and a uniform PUA cmap over every glyph.
  ASSERT_TRUE(sfnt::SfntFont::is_sfnt(otf));
  const sfnt::SfntFont wrapped{std::make_unique<std::istringstream>(otf)};
  EXPECT_EQ(wrapped.format(), FontFormat::opentype_cff);
  EXPECT_EQ(wrapped.glyph_count(), cff.glyph_count());
  // PUA code point U+E000+glyph maps back to that glyph.
  EXPECT_EQ(wrapped.glyph_for_code_point(pua_code_point(1)), 1);
  EXPECT_EQ(wrapped.glyph_for_code_point(pua_code_point(0)), 0);
  // The synthesized hmtx carries the CFF advance widths.
  EXPECT_EQ(wrapped.advance_width(1), 250);
}
