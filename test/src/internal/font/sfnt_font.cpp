#include <odr/internal/font/sfnt_font.hpp>

#include <odr/font.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::internal::font::sfnt;

namespace {

namespace bs = odr::internal::util::byte_string;

/// Parse a font from its in-memory bytes.
SfntFont sfnt_font_from_string(std::string bytes) {
  return SfntFont(std::move(bytes));
}

/// A `head` table: only unitsPerEm (offset 18) and the bbox (36..42) are read.
std::string head_table() {
  std::string t(54, '\0');
  const auto u16 = [&](std::size_t o, std::uint16_t v) {
    t[o] = static_cast<char>(v >> 8);
    t[o + 1] = static_cast<char>(v & 0xff);
  };
  u16(18, 1000);                             // unitsPerEm
  u16(36, static_cast<std::uint16_t>(-100)); // xMin
  u16(38, static_cast<std::uint16_t>(-200)); // yMin
  u16(40, 900);                              // xMax
  u16(42, 800);                              // yMax
  return t;
}

std::string maxp_table(const std::uint16_t glyphs) {
  std::string t;
  bs::put_u32_be(t, 0x00010000); // version 1.0
  bs::put_u16_be(t, glyphs);
  t.resize(32, '\0');
  return t;
}

std::string hhea_table(const std::uint16_t number_of_h_metrics) {
  std::string t(36, '\0');
  t[34] = static_cast<char>(number_of_h_metrics >> 8);
  t[35] = static_cast<char>(number_of_h_metrics & 0xff);
  return t;
}

std::string hmtx_table(const std::vector<std::uint16_t> &advances) {
  std::string t;
  for (const std::uint16_t a : advances) {
    bs::put_u16_be(t, a); // advanceWidth
    bs::put_u16_be(t, 0); // leftSideBearing
  }
  return t;
}

/// Format-4 subtable mapping the contiguous run [start, start+count) to glyph
/// ids [1, count] via a single idDelta segment, plus the required terminator.
std::string cmap_format4(const char16_t start, const std::uint16_t count) {
  std::string t;
  bs::put_u16_be(t, 4);  // format
  bs::put_u16_be(t, 32); // length
  bs::put_u16_be(t, 0);  // language
  bs::put_u16_be(t, 4);  // segCountX2 (2 segments)
  bs::put_u16_be(t, 0);  // searchRange
  bs::put_u16_be(t, 0);  // entrySelector
  bs::put_u16_be(t, 0);  // rangeShift
  bs::put_u16_be(t,
                 static_cast<std::uint16_t>(start + count - 1)); // endCode[0]
  bs::put_u16_be(t, 0xffff);                                     // endCode[1]
  bs::put_u16_be(t, 0);                                          // reservedPad
  bs::put_u16_be(t, start);                                      // startCode[0]
  bs::put_u16_be(t, 0xffff);                                     // startCode[1]
  bs::put_u16_be(
      t, static_cast<std::uint16_t>(1 - start)); // idDelta[0] -> gid 1..count
  bs::put_u16_be(t, 1);                          // idDelta[1]
  bs::put_u16_be(t, 0);                          // idRangeOffset[0]
  bs::put_u16_be(t, 0);                          // idRangeOffset[1]
  return t;
}

/// Format-4 subtable mapping [start, start+count) to glyph ids [1, count] via a
/// non-zero idRangeOffset[0] that indexes the glyphIdArray, plus the
/// terminator. idRangeOffset[0] == 4 points at glyphIdArray[0] (past the one
/// remaining idRangeOffset entry), and idDelta[0] == 0 takes the glyph id
/// straight from the array.
std::string cmap_format4_glyph_array(const char16_t start,
                                     const std::uint16_t count) {
  std::string t;
  bs::put_u16_be(t, 4);                                          // format
  bs::put_u16_be(t, static_cast<std::uint16_t>(32 + 2 * count)); // length
  bs::put_u16_be(t, 0);                                          // language
  bs::put_u16_be(t, 4);                                          // segCountX2
  bs::put_u16_be(t, 0);                                          // searchRange
  bs::put_u16_be(t, 0); // entrySelector
  bs::put_u16_be(t, 0); // rangeShift
  bs::put_u16_be(t,
                 static_cast<std::uint16_t>(start + count - 1)); // endCode[0]
  bs::put_u16_be(t, 0xffff);                                     // endCode[1]
  bs::put_u16_be(t, 0);                                          // reservedPad
  bs::put_u16_be(t, start);                                      // startCode[0]
  bs::put_u16_be(t, 0xffff);                                     // startCode[1]
  bs::put_u16_be(t, 0);                                          // idDelta[0]
  bs::put_u16_be(t, 1);                                          // idDelta[1]
  bs::put_u16_be(t, 4); // idRangeOffset[0] -> glyphIdArray[0]
  bs::put_u16_be(t, 0); // idRangeOffset[1]
  for (std::uint16_t i = 0; i < count; ++i) {
    bs::put_u16_be(t, static_cast<std::uint16_t>(i + 1)); // glyphIdArray
  }
  return t;
}

// Format-12 subtable: one group mapping [start, start+count) to [1, count].
std::string cmap_format12(const char32_t start, const std::uint32_t count) {
  std::string t;
  bs::put_u16_be(t, 12); // format
  bs::put_u16_be(t, 0);  // reserved
  bs::put_u32_be(t, 28); // length
  bs::put_u32_be(t, 0);  // language
  bs::put_u32_be(t, 1);  // nGroups
  bs::put_u32_be(t, start);
  bs::put_u32_be(t, start + count - 1);
  bs::put_u32_be(t, 1); // startGlyphID
  return t;
}

std::string cmap_table(const std::uint16_t platform,
                       const std::uint16_t encoding,
                       const std::string &subtable) {
  std::string t;
  bs::put_u16_be(t, 0); // version
  bs::put_u16_be(t, 1); // numTables
  bs::put_u16_be(t, platform);
  bs::put_u16_be(t, encoding);
  bs::put_u32_be(t, 12); // offset to the single subtable
  t += subtable;
  return t;
}

// A `name` table with a single PostScript-name (id 6) record, UTF-16BE.
std::string name_table(const std::string &ascii) {
  std::string strings;
  for (const char c : ascii) {
    bs::put_u16_be(strings, static_cast<std::uint8_t>(c));
  }
  std::string t;
  bs::put_u16_be(t, 0);     // format
  bs::put_u16_be(t, 1);     // count
  bs::put_u16_be(t, 18);    // stringOffset (6 header + 12 record)
  bs::put_u16_be(t, 3);     // platformID (Windows)
  bs::put_u16_be(t, 1);     // encodingID
  bs::put_u16_be(t, 0x409); // languageID
  bs::put_u16_be(t, 6);     // nameID (PostScript)
  bs::put_u16_be(t, static_cast<std::uint16_t>(strings.size()));
  bs::put_u16_be(t, 0); // offset within string storage
  t += strings;
  return t;
}

// Assemble a full SFNT from named tables, computing the table directory.
std::string
build_sfnt(const std::vector<std::pair<std::string, std::string>> &tables) {
  const auto count = static_cast<std::uint16_t>(tables.size());
  std::string out;
  bs::put_u32_be(out, 0x00010000); // sfntVersion (TrueType)
  bs::put_u16_be(out, count);
  bs::put_u16_be(out, 0); // searchRange
  bs::put_u16_be(out, 0); // entrySelector
  bs::put_u16_be(out, 0); // rangeShift

  std::uint32_t offset = 12 + count * 16U;
  std::string body;
  for (const auto &[tag, data] : tables) {
    out += tag;
    bs::put_u32_be(out, 0); // checksum (not validated by the reader)
    bs::put_u32_be(out, offset);
    bs::put_u32_be(out, static_cast<std::uint32_t>(data.size()));
    body += data;
    while (body.size() % 4 != 0) {
      body += '\0';
    }
    offset = 12 + count * 16U + static_cast<std::uint32_t>(body.size());
  }
  return out + body;
}

std::string sample_font(const std::string &cmap) {
  // Tables are stored in tag-sorted order, as required by the spec.
  return build_sfnt({{"cmap", cmap},
                     {"head", head_table()},
                     {"hhea", hhea_table(5)},
                     {"hmtx", hmtx_table({500, 600, 700, 222, 333})},
                     {"maxp", maxp_table(5)},
                     {"name", name_table("TestFont")}});
}

} // namespace

TEST(SfntFont, reads_facts) {
  const SfntFont font = sfnt_font_from_string(
      sample_font(cmap_table(3, 1, cmap_format4('A', 3))));

  EXPECT_EQ(font.format(), FontFormat::truetype);
  EXPECT_EQ(font.glyph_count(), 5);
  EXPECT_EQ(font.units_per_em(), 1000);
  EXPECT_FALSE(font.symbolic());
  EXPECT_EQ(font.name(), "TestFont");

  const FontBBox bbox = font.bounding_box();
  EXPECT_EQ(bbox.x_min, -100);
  EXPECT_EQ(bbox.y_min, -200);
  EXPECT_EQ(bbox.x_max, 900);
  EXPECT_EQ(bbox.y_max, 800);
}

TEST(SfntFont, advance_widths_with_monospace_tail) {
  const SfntFont font = sfnt_font_from_string(
      sample_font(cmap_table(3, 1, cmap_format4('A', 3))));

  EXPECT_EQ(font.advance_width(0), 500);
  EXPECT_EQ(font.advance_width(3), 222);
  EXPECT_EQ(font.advance_width(4), 333);
  // Glyphs beyond numberOfHMetrics share the last advance.
  EXPECT_EQ(font.advance_width(99), 333);
}

TEST(SfntFont, cmap_format4_forward_and_reverse) {
  const SfntFont font = sfnt_font_from_string(
      sample_font(cmap_table(3, 1, cmap_format4('A', 3))));

  EXPECT_EQ(font.glyph_for_code_point('A'), 1);
  EXPECT_EQ(font.glyph_for_code_point('C'), 3);
  EXPECT_EQ(font.glyph_for_code_point('Z'), 0); // unmapped -> .notdef

  EXPECT_EQ(font.code_point_for_glyph(1), U'A');
  EXPECT_EQ(font.code_point_for_glyph(3), U'C');
  EXPECT_FALSE(font.code_point_for_glyph(4).has_value()); // no code maps here
}

TEST(SfntFont, cmap_format4_glyph_id_array) {
  const SfntFont font = sfnt_font_from_string(
      sample_font(cmap_table(3, 1, cmap_format4_glyph_array('A', 3))));

  EXPECT_EQ(font.glyph_for_code_point('A'), 1);
  EXPECT_EQ(font.glyph_for_code_point('B'), 2);
  EXPECT_EQ(font.glyph_for_code_point('C'), 3);
  EXPECT_EQ(font.glyph_for_code_point('D'), 0); // outside the segment

  EXPECT_EQ(font.code_point_for_glyph(2), U'B');
}

TEST(SfntFont, cmap_format12_astral_plane) {
  const SfntFont font = sfnt_font_from_string(
      sample_font(cmap_table(3, 10, cmap_format12(0x1f600, 2))));

  EXPECT_EQ(font.glyph_for_code_point(0x1f600), 1);
  EXPECT_EQ(font.glyph_for_code_point(0x1f601), 2);
  EXPECT_EQ(font.code_point_for_glyph(2), static_cast<char32_t>(0x1f601));
}

TEST(SfntFont, symbolic_flag_from_platform_3_encoding_0) {
  const SfntFont font = sfnt_font_from_string(
      sample_font(cmap_table(3, 0, cmap_format4(0xf020, 3))));

  EXPECT_TRUE(font.symbolic());
  EXPECT_EQ(font.glyph_for_code_point(0xf020), 1);
}

TEST(SfntFont, is_sfnt) {
  EXPECT_TRUE(SfntFont::is_sfnt(std::string("\x00\x01\x00\x00", 4)));
  EXPECT_TRUE(SfntFont::is_sfnt("OTTO"));
  EXPECT_TRUE(SfntFont::is_sfnt("true"));
  EXPECT_TRUE(SfntFont::is_sfnt("ttcf"));
  EXPECT_FALSE(SfntFont::is_sfnt("%PDF"));
  EXPECT_FALSE(SfntFont::is_sfnt("ab"));
}

TEST(SfntFont, throws_on_truncated) {
  EXPECT_THROW(sfnt_font_from_string(std::string("\x00\x01\x00\x00", 4)),
               std::runtime_error);
}
