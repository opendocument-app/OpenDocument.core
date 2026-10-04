#include "sfnt_test_util.hpp"

#include <odr/internal/font/sfnt_font.hpp>

#include <odr/font.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

using namespace odr;
using namespace odr::internal::font::sfnt;

namespace {

using namespace odr::test::font;

namespace bs = odr::internal::util::byte_string;

/// Parse a font from its in-memory bytes.
SfntFont sfnt_font_from_string(std::string bytes) {
  return SfntFont(std::move(bytes));
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

std::string sample_font(const std::string &cmap) {
  // Tables are stored in tag-sorted order, as required by the spec.
  return sfnt_bytes({{"cmap", cmap},
                     {"head", head_table({-100, -200, 900, 800})},
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

TEST(SfntFont, write_pads_a_short_hmtx) {
  // Two longHorMetrics for five glyphs, and none of the three leftSideBearings
  // that must follow them.
  const SfntFont font = sfnt_font_from_string(
      sfnt_bytes({{"cmap", cmap_table(3, 1, cmap_format4('A', 3))},
                  {"head", head_table({-100, -200, 900, 800})},
                  {"hhea", hhea_table(2)},
                  {"hmtx", hmtx_table({500, 600})},
                  {"maxp", maxp_table(5)}}));

  const std::string written = font.write();
  const std::uint16_t count =
      bs::read_u16_be(std::string_view(written).substr(4));
  std::optional<std::uint32_t> hmtx_length;
  for (std::uint16_t i = 0; i < count; ++i) {
    const std::string_view entry =
        std::string_view(written).substr(12 + static_cast<std::size_t>(i) * 16);
    if (entry.substr(0, 4) == "hmtx") {
      hmtx_length = bs::read_u32_be(entry.substr(12));
    }
  }
  EXPECT_EQ(hmtx_length, 2 * 4 + 3 * 2);
  EXPECT_EQ(SfntFont(written).advance_width(4), 600);
}

TEST(SfntFont, rejects_invalid_headers_and_clips_quirky_ranges) {
  EXPECT_THROW(SfntFont(std::string(12, '\0')), std::runtime_error);
  std::string collection("ttcf");
  collection.resize(16, '\0');
  EXPECT_THROW(SfntFont{collection}, std::runtime_error);

  std::string bytes = sample_font(cmap_table(3, 1, cmap_format4('A', 3)));
  std::string clipped = bytes;
  bs::write_u32_be(clipped, 24, static_cast<std::uint32_t>(bytes.size()));
  EXPECT_EQ(SfntFont(clipped).glyph_for_code_point('B'), 2);
  bs::write_u32_be(bytes, 20, static_cast<std::uint32_t>(bytes.size() + 1));
  EXPECT_THROW(SfntFont{bytes}, std::runtime_error);

  std::string head = head_table({-100, -200, 900, 800});
  bs::write_u16_be(head, 18, 0);
  EXPECT_THROW(SfntFont(sfnt_bytes({{"head", head}})), std::runtime_error);

  std::string name = name_table("A");
  bs::write_u16_be(name, 14, 4);
  EXPECT_EQ(SfntFont(sfnt_bytes({{"name", name}})).name(), "A");
  bs::write_u16_be(name, 14, 1);
  EXPECT_EQ(SfntFont(sfnt_bytes({{"name", name}})).name(), "");
}

TEST(SfntFont, drops_an_invalid_cmap_and_keeps_the_font) {
  const std::string valid = cmap_format12(0x1f600, 2);
  const std::array<std::pair<std::size_t, std::uint32_t>, 5> patches{
      {{20, std::numeric_limits<std::uint32_t>::max()},
       {20, 0x1f5ff},
       {24, 0xFFFF},
       {12, 2},
       {4, 16}}};
  for (const auto &[offset, value] : patches) {
    std::string subtable = valid;
    bs::write_u32_be(subtable, offset, value);
    const SfntFont font(sample_font(cmap_table(3, 10, subtable)));
    EXPECT_TRUE(font.cmap().empty());
    EXPECT_EQ(font.advance_width(1), 600);
  }
  std::string overlapping = valid;
  overlapping += valid.substr(16);
  bs::write_u32_be(overlapping, 4,
                   static_cast<std::uint32_t>(overlapping.size()));
  bs::write_u32_be(overlapping, 12, 2);
  EXPECT_TRUE(
      SfntFont(sample_font(cmap_table(3, 10, overlapping))).cmap().empty());

  std::string short_subtable = cmap_format4('A', 3);
  bs::write_u16_be(short_subtable, 2, 16);
  EXPECT_TRUE(
      SfntFont(sample_font(cmap_table(3, 1, short_subtable))).cmap().empty());
  short_subtable = cmap_format4('A', 3);
  bs::write_u16_be(short_subtable, 6, 3);
  EXPECT_TRUE(
      SfntFont(sample_font(cmap_table(3, 1, short_subtable))).cmap().empty());
}

TEST(SfntFont, skips_a_format4_segment_out_of_order) {
  std::string subtable = cmap_format4('A', 3);
  bs::write_u16_be(subtable, 14, 'A' - 1); // endCode[0] below startCode[0]
  const SfntFont font(sample_font(cmap_table(3, 1, subtable)));
  EXPECT_EQ(font.glyph_for_code_point('A'), 0);
}

namespace {

/// One cmap with a (0, 6) record for @p first and a (3, 1) record for a valid
/// format-4 subtable after it.
std::string cmap_with_fallback(const std::string &first) {
  std::string cmap;
  bs::put_u16_be(cmap, 0);
  bs::put_u16_be(cmap, 2);
  bs::put_u16_be(cmap, 0);
  bs::put_u16_be(cmap, 6); // Unicode full repertoire.
  bs::put_u32_be(cmap, 20);
  bs::put_u16_be(cmap, 3);
  bs::put_u16_be(cmap, 1);
  bs::put_u32_be(cmap, static_cast<std::uint32_t>(20 + first.size()));
  cmap += first;
  std::string supported = cmap_format4('A', 3);
  bs::write_u16_be(supported, 30, 0xFFFF); // The terminator offset is unused.
  cmap += supported;
  return cmap;
}

} // namespace

TEST(SfntFont, unsupported_cmap_does_not_hide_a_supported_one) {
  std::string unsupported;
  bs::put_u16_be(unsupported, 13);
  EXPECT_EQ(SfntFont(sample_font(cmap_with_fallback(unsupported)))
                .glyph_for_code_point('B'),
            2);
  std::string broken = cmap_format12(0x1f600, 2);
  bs::write_u32_be(broken, 20, 0x1f5ff);
  EXPECT_EQ(SfntFont(sample_font(cmap_with_fallback(broken)))
                .glyph_for_code_point('B'),
            2);
}
