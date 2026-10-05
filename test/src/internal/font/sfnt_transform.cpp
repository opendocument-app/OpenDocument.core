#include <odr/internal/font/sfnt_transform.hpp>

#include <odr/internal/font/sfnt_font.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <internal/font/sfnt_test_util.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

using namespace odr::internal::font;

namespace {

using namespace odr::test::font;

namespace bs = odr::internal::util::byte_string;

/// Parse a font from its in-memory bytes.
sfnt::SfntFont parse(std::string bytes) {
  return sfnt::SfntFont(std::move(bytes));
}

/// Parse @p bytes, re-encode it to the PUA in place, and write it back out.
std::string reencoded(std::string bytes) {
  sfnt::SfntFont font = parse(std::move(bytes));
  reencode_to_pua(font);
  return font.write();
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

// The body of the named table in a serialized SFNT, located via the table
// directory (12-byte header + 16 bytes per entry: tag, checksum, offset,
// length). Returns nullopt when the tag is absent.
std::optional<std::string> table(const std::string &data,
                                 const std::string &tag) {
  const auto u16 = [&](const std::size_t at) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint8_t>(data[at]) << 8) |
        static_cast<std::uint8_t>(data[at + 1]));
  };
  const auto u32 = [&](const std::size_t at) {
    return static_cast<std::uint32_t>((u16(at) << 16) | u16(at + 2));
  };
  const std::uint16_t count = u16(4);
  for (std::size_t i = 0; i < count; ++i) {
    const std::size_t entry = 12 + i * 16;
    if (data.compare(entry, 4, tag) == 0) {
      return data.substr(u32(entry + 8), u32(entry + 12));
    }
  }
  return std::nullopt;
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
      build_sfnt(0x00010000, {{"head", head_table()},
                              {"maxp", maxp_table(6)},
                              {"hhea", hhea_table(0)},
                              {"cmap", serialize_cmap(map)}});

  const sfnt::SfntFont parsed = parse(font);
  EXPECT_EQ(parsed.glyph_for_code_point('A'), 1);
  EXPECT_EQ(parsed.glyph_for_code_point('B'), 2);
  EXPECT_EQ(parsed.glyph_for_code_point('Z'), 5);
  EXPECT_EQ(parsed.glyph_for_code_point('C'), 0); // gap between the segments
}

TEST(SfntTransform, serialize_cmap_format12_round_trips_beyond_bmp) {
  // A beyond-BMP code point forces a format-12 subtable. Mixes a BMP entry
  // ('A'), a Supplementary PUA-A run (two consecutive codes/glyphs), and a
  // lone high code point — exercising the group builder across all three.
  const std::map<char32_t, std::uint16_t> map{
      {'A', 1}, {0xf0000, 2}, {0xf0001, 3}, {0x10fffd, 9}};
  const std::string font =
      build_sfnt(0x00010000, {{"head", head_table()},
                              {"maxp", maxp_table(10)},
                              {"hhea", hhea_table(0)},
                              {"cmap", serialize_cmap(map)}});

  const sfnt::SfntFont parsed = parse(font);
  EXPECT_EQ(parsed.glyph_for_code_point('A'), 1);
  EXPECT_EQ(parsed.glyph_for_code_point(0xf0000), 2);
  EXPECT_EQ(parsed.glyph_for_code_point(0xf0001), 3);
  EXPECT_EQ(parsed.glyph_for_code_point(0x10fffd), 9);
  EXPECT_EQ(parsed.glyph_for_code_point(0xf0002), 0); // gap after the run
}

TEST(SfntTransform, reencode_mutates_and_round_trips) {
  sfnt::SfntFont font = parse(sample_font());
  reencode_to_pua(font);
  const auto check = [](const sfnt::SfntFont &mapped) {
    EXPECT_EQ(mapped.glyph_for_code_point(pua_code_point(1)), 1);
    EXPECT_EQ(mapped.glyph_for_code_point(pua_code_point(2)), 2);
    EXPECT_EQ(mapped.code_point_for_glyph(1), pua_code_point(1));
    EXPECT_EQ(mapped.code_point_for_glyph(2), pua_code_point(2));
  };
  check(font);
  check(parse(font.write()));
}

TEST(SfntTransform, reencode_with_extra_mutates_and_round_trips) {
  sfnt::SfntFont font = parse(sample_font());
  reencode_to_pua(font, {{U'A', 1}, {U'Z', 2}});
  const auto check = [](const sfnt::SfntFont &mapped) {
    EXPECT_EQ(mapped.glyph_for_code_point('A'), 1);
    EXPECT_EQ(mapped.glyph_for_code_point('Z'), 2);
    EXPECT_EQ(mapped.glyph_for_code_point(pua_code_point(0)), 0);
    EXPECT_EQ(mapped.glyph_for_code_point(pua_code_point(1)), 1);
    EXPECT_EQ(mapped.glyph_for_code_point(pua_code_point(2)), 2);
  };
  check(font);
  check(parse(font.write()));
}

TEST(SfntTransform, reencode_drops_extra_entries_past_glyph_count) {
  sfnt::SfntFont font = parse(sample_font()); // 3 glyphs: valid ids 0..2
  // 'A' -> 1 is in range; 'B' -> 7 is past the glyph count. An out-of-range
  // glyph reference makes the OTS sanitizer reject the whole cmap (and the
  // font), so it must be dropped rather than written.
  reencode_to_pua(font, {{U'A', 1}, {U'B', 7}});

  EXPECT_EQ(font.glyph_for_code_point('A'), 1);
  EXPECT_EQ(font.glyph_for_code_point('B'), 0); // dropped: not mapped
  // The in-range PUA range is untouched.
  EXPECT_EQ(font.glyph_for_code_point(pua_code_point(2)), 2);
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

TEST(SfntTransform, reencode_overflows_into_supplementary_pua) {
  // A font with more glyphs than the 6400-slot BMP PUA re-encodes by spilling
  // the overflow into Supplementary PUA-A; the writer emits a format-12 cmap so
  // the beyond-BMP code points round-trip.
  sfnt::SfntFont font = parse(sample_font(7000));
  reencode_to_pua(font);

  // Glyph 0 stays in the BMP PUA; glyph 6400 is the first overflow into PUA-A.
  EXPECT_EQ(pua_code_point(0), 0xe000u);
  EXPECT_EQ(pua_code_point(6400), 0xf0000u);

  const sfnt::SfntFont parsed = parse(font.write());
  EXPECT_EQ(parsed.glyph_for_code_point(pua_code_point(0)), 0);
  EXPECT_EQ(parsed.glyph_for_code_point(pua_code_point(6399)), 6399);
  EXPECT_EQ(parsed.glyph_for_code_point(pua_code_point(6400)), 6400);
  EXPECT_EQ(parsed.glyph_for_code_point(pua_code_point(6999)), 6999);
}

TEST(SfntTransform, write_synthesizes_post_when_absent) {
  // `sample_font()` carries no `post` table; the writer must add a minimal
  // format-3.0 one (OTS rejects the whole font otherwise, so browsers drop the
  // `@font-face` and render tofu).
  ASSERT_FALSE(table(sample_font(), "post").has_value());

  const std::string out = reencoded(sample_font());
  const std::optional<std::string> post = table(out, "post");
  ASSERT_TRUE(post.has_value());
  EXPECT_EQ(post->size(), 32u);
  EXPECT_EQ(post->substr(0, 4), std::string("\x00\x03\x00\x00", 4)); // v3.0
  EXPECT_EQ(file_checksum(out), 0xb1b0afbaU);
}

TEST(SfntTransform, write_keeps_existing_post) {
  std::string original_post(32, '\0');
  original_post[1] = 0x02; // version 2.0 marker, distinct from the synthesized
  const std::string font = build_sfnt(0x00010000, {{"head", head_table()},
                                                   {"maxp", maxp_table(3)},
                                                   {"hhea", hhea_table(0)},
                                                   {"post", original_post}});

  sfnt::SfntFont parsed = parse(font);
  reencode_to_pua(parsed);
  const std::string out = parsed.write();

  // The source `post` is copied through verbatim, not replaced.
  EXPECT_EQ(table(out, "post"), original_post);
}

TEST(SfntTransform, write_drops_tables_it_does_not_need) {
  // A malformed layout table costs the whole font at the sanitizer, and nothing
  // here needs one; the outlines and their hinting must survive.
  const std::string font = build_sfnt(0x00010000, {{"head", head_table()},
                                                   {"maxp", maxp_table(3)},
                                                   {"hhea", hhea_table(0)},
                                                   {"prep", "hint"},
                                                   {"GSUB", "junk"},
                                                   {"DSIG", "junk"},
                                                   {"FFTM", "junk"}});

  const std::string out = reencoded(font);
  EXPECT_EQ(table(out, "prep"), "hint");
  EXPECT_FALSE(table(out, "GSUB").has_value());
  EXPECT_FALSE(table(out, "DSIG").has_value());
  EXPECT_FALSE(table(out, "FFTM").has_value());
}

TEST(SfntTransform, write_synthesizes_name_when_absent) {
  // OTS requires `name` and TrueType subsets often drop it; the writer must
  // synthesize a minimal one (empty source name falls back to "ODR Font").
  const std::string font = build_sfnt(0x00010000, {{"head", head_table()},
                                                   {"maxp", maxp_table(3)},
                                                   {"hhea", hhea_table(0)}});
  ASSERT_FALSE(table(font, "name").has_value());

  const std::string out = reencoded(font);
  const std::optional<std::string> name = table(out, "name");
  ASSERT_TRUE(name.has_value());
  EXPECT_EQ(parse(out).name(), "ODR-Font");
  EXPECT_EQ(file_checksum(out), 0xb1b0afbaU);
}

TEST(SfntTransform, write_keeps_existing_name) {
  // `sample_font()` carries a `name` table; it is copied through verbatim.
  const std::string out = reencoded(sample_font());
  EXPECT_EQ(table(out, "name"), name_table("TestFont"));
}

TEST(SfntTransform, write_synthesizes_os2_when_absent) {
  // `sample_font()` carries no `OS/2` table; OTS requires one, so the writer
  // must synthesize a version-4 table for the `@font-face` to be accepted.
  ASSERT_FALSE(table(sample_font(), "OS/2").has_value());

  const std::string out = reencoded(sample_font());
  const std::optional<std::string> os2 = table(out, "OS/2");
  ASSERT_TRUE(os2.has_value());
  EXPECT_EQ(os2->size(), 96u);
  const auto u16 = [&](const std::size_t at) {
    return static_cast<std::uint16_t>(
        (static_cast<std::uint8_t>((*os2)[at]) << 8) |
        static_cast<std::uint8_t>((*os2)[at + 1]));
  };
  EXPECT_EQ(u16(0), 4u);   // version 4
  EXPECT_EQ(u16(4), 400u); // usWeightClass: regular
  EXPECT_EQ(u16(6), 5u);   // usWidthClass: medium
  EXPECT_EQ(u16(64),
            0xe000); // usFirstCharIndex: PUA base (3 glyphs re-encoded)
  EXPECT_EQ(u16(66), 0xe002); // usLastCharIndex
  EXPECT_EQ(file_checksum(out), 0xb1b0afbaU);
}

TEST(SfntTransform, write_keeps_existing_os2) {
  std::string original_os2(96, '\0');
  original_os2[1] = 0x02; // version 2 marker, distinct from the synthesized 4
  const std::string font = build_sfnt(0x00010000, {{"head", head_table()},
                                                   {"maxp", maxp_table(3)},
                                                   {"hhea", hhea_table(0)},
                                                   {"OS/2", original_os2}});

  sfnt::SfntFont parsed = parse(font);
  reencode_to_pua(parsed);
  const std::string out = parsed.write();

  // The source `OS/2` is copied through verbatim, not replaced.
  EXPECT_EQ(table(out, "OS/2"), original_os2);
}

TEST(SfntTransform, cmap_format_tracks_encoded_size_and_unicode_range) {
  std::map<char32_t, std::uint16_t> mapping;
  for (char32_t code = 0; code < 8188 * 2; code += 2) {
    mapping.emplace(code, 1);
  }
  EXPECT_EQ(bs::read_u16_be(serialize_cmap(mapping).substr(12)), 4);
  mapping.emplace(8188 * 2, 1);
  const std::string large = serialize_cmap(mapping);
  EXPECT_EQ(bs::read_u16_be(large.substr(12)), 12);
  EXPECT_EQ(parse(build_sfnt(0x00010000, {{"cmap", large}})).cmap(), mapping);

  std::map<char32_t, std::uint16_t> edge{{0xFFFF, 1}};
  EXPECT_EQ(bs::read_u16_be(serialize_cmap(edge).substr(12)), 12);
  edge.emplace(0x10FFFF, 2);
  const std::string cmap = serialize_cmap(edge);
  EXPECT_EQ(parse(build_sfnt(0x00010000, {{"cmap", cmap}})).cmap(), edge);
  EXPECT_THROW((void)serialize_cmap({{0x110000, 1}}), std::runtime_error);
}

TEST(SfntTransform, names_preserve_unicode_and_bound_postscript_names) {
  const std::string name = serialize_name("Fönt / 😀");
  const std::uint16_t storage = bs::read_u16_be(name.substr(4));
  const auto value = [&](const std::size_t index) {
    const std::size_t record = 6 + index * 12;
    return name.substr(storage + bs::read_u16_be(name.substr(record + 10)),
                       bs::read_u16_be(name.substr(record + 8)));
  };
  const std::string expected("\0F\0\xf6\0n\0t\0 \0/\0 \xd8\x3d\xde\0", 18);
  EXPECT_EQ(value(0), expected);
  EXPECT_EQ(value(2), expected);
  EXPECT_EQ(parse(build_sfnt(0x00010000, {{"name", name}})).name(),
            "F-nt-----");
  const std::string long_name = serialize_name(std::string(30000, 'A'));
  EXPECT_EQ(parse(build_sfnt(0x00010000, {{"name", long_name}})).name(),
            std::string(63, 'A'));
  EXPECT_LT(long_name.size(), 61000);
  EXPECT_THROW((void)serialize_name(std::string(32768, 'A')),
               std::runtime_error);
  EXPECT_EQ(
      parse(build_sfnt(0x00010000, {{"name", serialize_name("A\xff")}})).name(),
      "A-");
}

TEST(SfntTransform, rejects_unrepresentable_table_directories) {
  EXPECT_THROW((void)build_sfnt(0x00010000, {{"abc", ""}}), std::runtime_error);
  EXPECT_THROW((void)build_sfnt(0x00010000, {{"head", ""}, {"head", ""}}),
               std::runtime_error);
  const std::vector<std::pair<std::string, std::string>> tables(4096);
  EXPECT_THROW((void)build_sfnt(0x00010000, tables), std::runtime_error);
}
