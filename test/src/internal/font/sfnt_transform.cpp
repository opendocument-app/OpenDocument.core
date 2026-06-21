#include <odr/internal/font/sfnt_transform.hpp>

#include <odr/internal/font/sfnt_font.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace odr::internal::font;

namespace {

/// Parse a font from its in-memory bytes (the reader consumes an
/// `std::istream`).
sfnt::SfntFont parse(std::string bytes) {
  return sfnt::SfntFont(std::make_unique<std::istringstream>(std::move(bytes)));
}

/// Serialize an SFNT to bytes (the writer emits to an `std::ostream`).
std::string
build_font(std::uint32_t version,
           std::vector<std::pair<std::string, std::string>> tables) {
  std::ostringstream out;
  build_sfnt(out, version, std::move(tables));
  return out.str();
}

/// Parse @p bytes, re-encode it to the PUA in place, and write it back out.
std::string reencoded(std::string bytes) {
  sfnt::SfntFont font = parse(std::move(bytes));
  reencode_to_pua(font);
  std::ostringstream out;
  font.write(out);
  return out.str();
}

void put16(std::string &s, const std::uint16_t v) {
  s += static_cast<char>(v >> 8);
  s += static_cast<char>(v & 0xff);
}

void put32(std::string &s, const std::uint32_t v) {
  put16(s, static_cast<std::uint16_t>(v >> 16));
  put16(s, static_cast<std::uint16_t>(v & 0xffff));
}

std::string head_table() {
  std::string t(54, '\0');
  t[18] = 0x03; // unitsPerEm = 1000 (0x03E8)
  t[19] = static_cast<char>(0xe8);
  return t;
}

std::string maxp_table(const std::uint16_t glyphs) {
  std::string t;
  put32(t, 0x00010000);
  put16(t, glyphs);
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
    put16(t, a);
    put16(t, 0);
  }
  return t;
}

std::string name_table(const std::string &ascii) {
  std::string strings;
  for (const char c : ascii) {
    put16(strings, static_cast<std::uint8_t>(c));
  }
  std::string t;
  put16(t, 0);
  put16(t, 1);
  put16(t, 18);
  put16(t, 3);
  put16(t, 1);
  put16(t, 0x409);
  put16(t, 6);
  put16(t, static_cast<std::uint16_t>(strings.size()));
  put16(t, 0);
  t += strings;
  return t;
}

// A 3-glyph TrueType font (no original cmap — the re-encode supplies one),
// assembled through the library's own serializer.
std::string sample_font(std::uint16_t glyphs = 3) {
  return build_font(0x00010000, {{"head", head_table()},
                                 {"maxp", maxp_table(glyphs)},
                                 {"hhea", hhea_table(3)},
                                 {"hmtx", hmtx_table({500, 600, 700})},
                                 {"name", name_table("TestFont")}});
}

// Whole-file SFNT checksum (independent of the writer's), which must equal the
// magic constant once head.checkSumAdjustment is set.
std::uint32_t file_checksum(const std::string &data) {
  std::uint32_t sum = 0;
  for (std::size_t i = 0; i < data.size(); i += 4) {
    std::uint32_t word = 0;
    for (std::size_t b = 0; b < 4; ++b) {
      word = word << 8 |
             (i + b < data.size() ? static_cast<std::uint8_t>(data[i + b]) : 0);
    }
    sum += word;
  }
  return sum;
}

} // namespace

TEST(SfntTransform, pua_code_point_is_deterministic) {
  EXPECT_EQ(pua_code_point(0), static_cast<char32_t>(0xe000));
  EXPECT_EQ(pua_code_point(5), static_cast<char32_t>(0xe005));
}

TEST(SfntTransform, build_sfnt_has_valid_checksum_and_parses) {
  const std::string font = sample_font();

  // A correct head.checkSumAdjustment makes the whole-file checksum the magic.
  EXPECT_EQ(file_checksum(font), 0xb1b0afbaU);

  const sfnt::SfntFont parsed = parse(font);
  EXPECT_EQ(parsed.glyph_count(), 3);
  EXPECT_EQ(parsed.units_per_em(), 1000);
  EXPECT_EQ(parsed.name(), "TestFont");
}

TEST(SfntTransform, serialize_cmap_round_trips_multiple_segments) {
  // 'A','B' form one arithmetic run (codes and glyphs consecutive); 'Z' is a
  // second segment — exercises the segment builder beyond a single run.
  const std::map<char32_t, std::uint16_t> map{{'A', 1}, {'B', 2}, {'Z', 5}};
  const std::string font =
      build_font(0x00010000, {{"head", head_table()},
                              {"maxp", maxp_table(6)},
                              {"hhea", hhea_table(0)},
                              {"cmap", serialize_cmap(map)}});

  const sfnt::SfntFont parsed = parse(font);
  EXPECT_EQ(parsed.glyph_for_code_point('A'), 1);
  EXPECT_EQ(parsed.glyph_for_code_point('B'), 2);
  EXPECT_EQ(parsed.glyph_for_code_point('Z'), 5);
  EXPECT_EQ(parsed.glyph_for_code_point('C'), 0); // gap between the segments
}

TEST(SfntTransform, serialize_cmap_rejects_beyond_bmp) {
  const std::map<char32_t, std::uint16_t> map{{0x1f600, 1}};
  EXPECT_THROW((void)serialize_cmap(map), std::runtime_error);
}

TEST(SfntTransform, reencode_mutates_the_font_in_place) {
  sfnt::SfntFont font = parse(sample_font());
  reencode_to_pua(font);

  // The mutated object itself reflects the new map — accessors read cmap().
  EXPECT_EQ(font.glyph_for_code_point(pua_code_point(1)), 1);
  EXPECT_EQ(font.glyph_for_code_point(pua_code_point(2)), 2);
  EXPECT_EQ(font.code_point_for_glyph(1), pua_code_point(1));
  EXPECT_EQ(font.code_point_for_glyph(2), pua_code_point(2));
}

TEST(SfntTransform, reencode_maps_every_glyph_to_pua_after_round_trip) {
  const sfnt::SfntFont parsed = parse(reencoded(sample_font()));

  EXPECT_EQ(parsed.glyph_for_code_point(pua_code_point(1)), 1);
  EXPECT_EQ(parsed.glyph_for_code_point(pua_code_point(2)), 2);
  EXPECT_EQ(parsed.code_point_for_glyph(1), pua_code_point(1));
  EXPECT_EQ(parsed.code_point_for_glyph(2), pua_code_point(2));
}

TEST(SfntTransform, write_preserves_passthrough_tables_and_checksum) {
  const std::string out = reencoded(sample_font());

  EXPECT_EQ(file_checksum(out), 0xb1b0afbaU);

  const sfnt::SfntFont parsed = parse(out);
  EXPECT_EQ(parsed.glyph_count(), 3);
  EXPECT_EQ(parsed.units_per_em(), 1000);
  EXPECT_EQ(parsed.advance_width(0), 500);
  EXPECT_EQ(parsed.advance_width(2), 700);
  EXPECT_EQ(parsed.name(), "TestFont");
}

TEST(SfntTransform, reencode_rejects_too_many_glyphs) {
  sfnt::SfntFont font = parse(sample_font(7000));
  EXPECT_THROW(reencode_to_pua(font), std::runtime_error);
}
