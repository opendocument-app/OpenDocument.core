#include <odr/internal/font/sfnt_font.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace odr::internal::font;

namespace {

void put16(std::string &s, std::uint16_t v) {
  s += static_cast<char>(v >> 8);
  s += static_cast<char>(v & 0xff);
}

void put32(std::string &s, std::uint32_t v) {
  put16(s, static_cast<std::uint16_t>(v >> 16));
  put16(s, static_cast<std::uint16_t>(v & 0xffff));
}

// A `head` table: only unitsPerEm (offset 18) and the bbox (36..42) are read.
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

std::string maxp_table(std::uint16_t glyphs) {
  std::string t;
  put32(t, 0x00010000); // version 1.0
  put16(t, glyphs);
  t.resize(32, '\0');
  return t;
}

std::string hhea_table(std::uint16_t number_of_h_metrics) {
  std::string t(36, '\0');
  t[34] = static_cast<char>(number_of_h_metrics >> 8);
  t[35] = static_cast<char>(number_of_h_metrics & 0xff);
  return t;
}

std::string hmtx_table(const std::vector<std::uint16_t> &advances) {
  std::string t;
  for (const std::uint16_t a : advances) {
    put16(t, a); // advanceWidth
    put16(t, 0); // leftSideBearing
  }
  return t;
}

// Format-4 subtable mapping the contiguous run [start, start+count) to glyph
// ids [1, count] via a single idDelta segment, plus the required terminator.
std::string cmap_format4(char16_t start, std::uint16_t count) {
  std::string t;
  put16(t, 4);  // format
  put16(t, 32); // length
  put16(t, 0);  // language
  put16(t, 4);  // segCountX2 (2 segments)
  put16(t, 0);  // searchRange
  put16(t, 0);  // entrySelector
  put16(t, 0);  // rangeShift
  put16(t, static_cast<std::uint16_t>(start + count - 1)); // endCode[0]
  put16(t, 0xffff);                                        // endCode[1]
  put16(t, 0);                                             // reservedPad
  put16(t, start);                                         // startCode[0]
  put16(t, 0xffff);                                        // startCode[1]
  put16(t, static_cast<std::uint16_t>(1 - start)); // idDelta[0] -> gid 1..count
  put16(t, 1);                                     // idDelta[1]
  put16(t, 0);                                     // idRangeOffset[0]
  put16(t, 0);                                     // idRangeOffset[1]
  return t;
}

// Format-12 subtable: one group mapping [start, start+count) to [1, count].
std::string cmap_format12(char32_t start, std::uint32_t count) {
  std::string t;
  put16(t, 12); // format
  put16(t, 0);  // reserved
  put32(t, 28); // length
  put32(t, 0);  // language
  put32(t, 1);  // nGroups
  put32(t, start);
  put32(t, start + count - 1);
  put32(t, 1); // startGlyphID
  return t;
}

std::string cmap_table(std::uint16_t platform, std::uint16_t encoding,
                       const std::string &subtable) {
  std::string t;
  put16(t, 0); // version
  put16(t, 1); // numTables
  put16(t, platform);
  put16(t, encoding);
  put32(t, 12); // offset to the single subtable
  t += subtable;
  return t;
}

// A `name` table with a single PostScript-name (id 6) record, UTF-16BE.
std::string name_table(const std::string &ascii) {
  std::string strings;
  for (const char c : ascii) {
    put16(strings, static_cast<std::uint8_t>(c));
  }
  std::string t;
  put16(t, 0);     // format
  put16(t, 1);     // count
  put16(t, 18);    // stringOffset (6 header + 12 record)
  put16(t, 3);     // platformID (Windows)
  put16(t, 1);     // encodingID
  put16(t, 0x409); // languageID
  put16(t, 6);     // nameID (PostScript)
  put16(t, static_cast<std::uint16_t>(strings.size()));
  put16(t, 0); // offset within string storage
  t += strings;
  return t;
}

// Assemble a full SFNT from named tables, computing the table directory.
std::string
build_sfnt(const std::vector<std::pair<std::string, std::string>> &tables) {
  const auto count = static_cast<std::uint16_t>(tables.size());
  std::string out;
  put32(out, 0x00010000); // sfntVersion (TrueType)
  put16(out, count);
  put16(out, 0); // searchRange
  put16(out, 0); // entrySelector
  put16(out, 0); // rangeShift

  std::uint32_t offset = 12 + count * 16U;
  std::string body;
  for (const auto &[tag, data] : tables) {
    out += tag;
    put32(out, 0); // checksum (not validated by the reader)
    put32(out, offset);
    put32(out, static_cast<std::uint32_t>(data.size()));
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

TEST(SfntFontProgram, reads_facts) {
  const SfntFontProgram font(
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

TEST(SfntFontProgram, advance_widths_with_monospace_tail) {
  const SfntFontProgram font(
      sample_font(cmap_table(3, 1, cmap_format4('A', 3))));

  EXPECT_EQ(font.advance_width(0), 500);
  EXPECT_EQ(font.advance_width(3), 222);
  EXPECT_EQ(font.advance_width(4), 333);
  // Glyphs beyond numberOfHMetrics share the last advance.
  EXPECT_EQ(font.advance_width(99), 333);
}

TEST(SfntFontProgram, cmap_format4_forward_and_reverse) {
  const SfntFontProgram font(
      sample_font(cmap_table(3, 1, cmap_format4('A', 3))));

  EXPECT_EQ(font.glyph_for_code_point('A'), 1);
  EXPECT_EQ(font.glyph_for_code_point('C'), 3);
  EXPECT_EQ(font.glyph_for_code_point('Z'), 0); // unmapped -> .notdef

  EXPECT_EQ(font.code_point_for_glyph(1), U'A');
  EXPECT_EQ(font.code_point_for_glyph(3), U'C');
  EXPECT_FALSE(font.code_point_for_glyph(4).has_value()); // no code maps here
}

TEST(SfntFontProgram, cmap_format12_astral_plane) {
  const SfntFontProgram font(
      sample_font(cmap_table(3, 10, cmap_format12(0x1f600, 2))));

  EXPECT_EQ(font.glyph_for_code_point(0x1f600), 1);
  EXPECT_EQ(font.glyph_for_code_point(0x1f601), 2);
  EXPECT_EQ(font.code_point_for_glyph(2), static_cast<char32_t>(0x1f601));
}

TEST(SfntFontProgram, symbolic_flag_from_platform_3_encoding_0) {
  const SfntFontProgram font(
      sample_font(cmap_table(3, 0, cmap_format4(0xf020, 3))));

  EXPECT_TRUE(font.symbolic());
  EXPECT_EQ(font.glyph_for_code_point(0xf020), 1);
}

TEST(SfntFontProgram, table_lookup) {
  const SfntFontProgram font(
      sample_font(cmap_table(3, 1, cmap_format4('A', 3))));

  EXPECT_TRUE(font.table("cmap").has_value());
  EXPECT_TRUE(font.table("head").has_value());
  EXPECT_FALSE(font.table("CFF ").has_value());
}

TEST(SfntFontProgram, is_sfnt) {
  EXPECT_TRUE(SfntFontProgram::is_sfnt(std::string("\x00\x01\x00\x00", 4)));
  EXPECT_TRUE(SfntFontProgram::is_sfnt("OTTO"));
  EXPECT_TRUE(SfntFontProgram::is_sfnt("true"));
  EXPECT_TRUE(SfntFontProgram::is_sfnt("ttcf"));
  EXPECT_FALSE(SfntFontProgram::is_sfnt("%PDF"));
  EXPECT_FALSE(SfntFontProgram::is_sfnt("ab"));
}

TEST(SfntFontProgram, throws_on_truncated) {
  EXPECT_THROW(SfntFontProgram(std::string("\x00\x01\x00\x00", 4)),
               std::runtime_error);
}
