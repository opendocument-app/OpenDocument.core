#include <odr/internal/font/otf_writer.hpp>

#include <odr/internal/font/sfnt_font.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace odr::internal::font;

namespace {

/// Parse a font from its in-memory bytes (the reader consumes an
/// `std::istream`).
sfnt::SfntFont parse(std::string bytes) {
  return sfnt::SfntFont(std::make_unique<std::istringstream>(std::move(bytes)));
}

void put16(std::string &s, std::uint16_t v) {
  s += static_cast<char>(v >> 8);
  s += static_cast<char>(v & 0xff);
}

void put32(std::string &s, std::uint32_t v) {
  put16(s, static_cast<std::uint16_t>(v >> 16));
  put16(s, static_cast<std::uint16_t>(v & 0xffff));
}

std::string head_table() {
  std::string t(54, '\0');
  t[18] = 0x03; // unitsPerEm = 1000 (0x03E8)
  t[19] = static_cast<char>(0xe8);
  return t;
}

std::string maxp_table(std::uint16_t glyphs) {
  std::string t;
  put32(t, 0x00010000);
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
  return build_sfnt(0x00010000, {{"head", head_table()},
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

TEST(OtfWriter, pua_code_point_is_deterministic) {
  EXPECT_EQ(pua_code_point(0), static_cast<char32_t>(0xe000));
  EXPECT_EQ(pua_code_point(5), static_cast<char32_t>(0xe005));
}

TEST(OtfWriter, build_sfnt_has_valid_checksum_and_sorted_directory) {
  const std::string font = sample_font();

  // A correct head.checkSumAdjustment makes the whole-file checksum the magic.
  EXPECT_EQ(file_checksum(font), 0xb1b0afbaU);

  const sfnt::SfntFont parsed = parse(font);
  EXPECT_EQ(parsed.glyph_count(), 3);
  EXPECT_EQ(parsed.units_per_em(), 1000);
  EXPECT_EQ(parsed.name(), "TestFont");
}

TEST(OtfWriter, reencode_maps_every_glyph_to_pua) {
  const sfnt::SfntFont parsed = parse(reencode_to_pua(parse(sample_font())));

  // Glyphs are addressable at their deterministic PUA code points.
  EXPECT_EQ(parsed.glyph_for_code_point(pua_code_point(1)), 1);
  EXPECT_EQ(parsed.glyph_for_code_point(pua_code_point(2)), 2);
  EXPECT_EQ(parsed.code_point_for_glyph(1), pua_code_point(1));
  EXPECT_EQ(parsed.code_point_for_glyph(2), pua_code_point(2));
}

TEST(OtfWriter, reencode_preserves_passthrough_tables_and_checksum) {
  const std::string out = reencode_to_pua(parse(sample_font()));

  EXPECT_EQ(file_checksum(out), 0xb1b0afbaU);

  const sfnt::SfntFont parsed = parse(out);
  EXPECT_EQ(parsed.glyph_count(), 3);
  EXPECT_EQ(parsed.units_per_em(), 1000);
  EXPECT_EQ(parsed.advance_width(0), 500);
  EXPECT_EQ(parsed.advance_width(2), 700);
  EXPECT_EQ(parsed.name(), "TestFont");
  EXPECT_TRUE(parsed.table("cmap").has_value());
}

TEST(OtfWriter, reencode_rejects_too_many_glyphs) {
  EXPECT_THROW(reencode_to_pua(parse(sample_font(7000))),
               std::runtime_error);
}
