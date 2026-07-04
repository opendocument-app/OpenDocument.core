#include <odr/internal/abstract/font.hpp>
#include <odr/internal/font/sfnt_font.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

using namespace odr::internal;
using odr::internal::pdf::Font;

namespace {

namespace bs = odr::internal::util::byte_string;

// A compact SFNT builder, just enough for an `SfntFont` that maps a contiguous
// run of code points to glyph ids 1..n (mirrors the font unit test's builder).

std::string head_table() {
  std::string t(54, '\0');
  t[18] = 0x03;
  t[19] = static_cast<char>(0xe8); // unitsPerEm = 1000
  return t;
}

std::string maxp_table(const std::uint16_t glyphs) {
  std::string t;
  bs::put_u32_be(t, 0x00010000);
  bs::put_u16_be(t, glyphs);
  t.resize(32, '\0');
  return t;
}

std::string hhea_table(const std::uint16_t h_metrics) {
  std::string t(36, '\0');
  t[34] = static_cast<char>(h_metrics >> 8);
  t[35] = static_cast<char>(h_metrics & 0xff);
  return t;
}

std::string hmtx_table(const std::uint16_t count) {
  std::string t;
  for (std::uint16_t i = 0; i < count; ++i) {
    bs::put_u16_be(t, 500);
    bs::put_u16_be(t, 0);
  }
  return t;
}

/// Format-4 (3,1) subtable mapping [start, start+count) to glyph ids [1,
/// count].
std::string cmap_format4(const char16_t start, const std::uint16_t count) {
  std::string t;
  bs::put_u16_be(t, 4);
  bs::put_u16_be(t, 32);
  bs::put_u16_be(t, 0);
  bs::put_u16_be(t, 4); // segCountX2
  bs::put_u16_be(t, 0);
  bs::put_u16_be(t, 0);
  bs::put_u16_be(t, 0);
  bs::put_u16_be(t,
                 static_cast<std::uint16_t>(start + count - 1)); // endCode[0]
  bs::put_u16_be(t, 0xffff);
  bs::put_u16_be(t, 0);
  bs::put_u16_be(t, start); // startCode[0]
  bs::put_u16_be(t, 0xffff);
  bs::put_u16_be(t, static_cast<std::uint16_t>(1 - start)); // idDelta[0]
  bs::put_u16_be(t, 1);
  bs::put_u16_be(t, 0); // idRangeOffset[0]
  bs::put_u16_be(t, 0);
  return t;
}

std::string cmap_table(const std::string &subtable) {
  std::string t;
  bs::put_u16_be(t, 0);
  bs::put_u16_be(t, 1);
  bs::put_u16_be(t, 3); // Windows
  bs::put_u16_be(t, 1); // Unicode BMP
  bs::put_u32_be(t, 12);
  t += subtable;
  return t;
}

std::string
build_sfnt(const std::vector<std::pair<std::string, std::string>> &tables) {
  const auto count = static_cast<std::uint16_t>(tables.size());
  std::string out;
  bs::put_u32_be(out, 0x00010000);
  bs::put_u16_be(out, count);
  bs::put_u16_be(out, 0);
  bs::put_u16_be(out, 0);
  bs::put_u16_be(out, 0);
  std::uint32_t offset = 12 + count * 16U;
  std::string body;
  for (const auto &[tag, data] : tables) {
    out += tag;
    bs::put_u32_be(out, 0);
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

/// A 5-glyph font whose `cmap` maps 'A','B','C' (U+0041..0043) to gids 1,2,3.
std::shared_ptr<abstract::Font> sample_font() {
  std::string sfnt = build_sfnt({{"cmap", cmap_table(cmap_format4('A', 3))},
                                 {"head", head_table()},
                                 {"hhea", hhea_table(5)},
                                 {"hmtx", hmtx_table(5)},
                                 {"maxp", maxp_table(5)}});
  return std::make_shared<font::sfnt::SfntFont>(std::move(sfnt));
}

/// Big-endian 2-byte codes, as a composite font's character codes appear.
std::string codes2(const std::vector<std::uint16_t> &cids) {
  std::string s;
  for (const std::uint16_t cid : cids) {
    bs::put_u16_be(s, cid);
  }
  return s;
}

} // namespace

TEST(PdfFont, composite_identity_glyph_for_code) {
  Font font;
  font.composite = true;
  font.embedded_font = sample_font();

  // Identity-H with an Identity /CIDToGIDMap: GID = CID = code.
  EXPECT_EQ(font.glyph_for_code(1), 1);
  EXPECT_EQ(font.glyph_for_code(3), 3);
}

TEST(PdfFont, composite_cid_to_gid_map) {
  Font font;
  font.composite = true;
  font.embedded_font = sample_font();
  // CID 1 -> GID 3, CID 2 -> GID 1.
  font.cid_to_gid = {0, 3, 1};

  EXPECT_EQ(font.glyph_for_code(1), 3);
  EXPECT_EQ(font.glyph_for_code(2), 1);
  EXPECT_EQ(font.glyph_for_code(9), 0); // out of range -> .notdef
}

TEST(PdfFont, embedded_reverse_map_closes_the_gap) {
  // A composite font with no /ToUnicode and no predefined CMap name: extraction
  // was "no Unicode" before stage 3.3; now the embedded font's reverse map
  // recovers it (CID 1 -> GID 1 -> 'A', etc.).
  Font font;
  font.composite = true;
  font.embedded_font = sample_font();

  EXPECT_EQ(font.to_unicode(codes2({1, 2, 3})), "ABC");
  // A glyph the cmap never reaches stays unmapped (gid 4 has no code point).
  EXPECT_EQ(font.to_unicode(codes2({4})), "");
}

TEST(PdfFont, to_unicode_prefers_cmap_over_reverse_map) {
  // An explicit /ToUnicode CMap wins over the embedded reverse map.
  Font font;
  font.composite = true;
  font.embedded_font = sample_font();
  font.cmap.add_codespace_range(codes2({0}), codes2({0xffff}));
  font.cmap.map_single(codes2({1}), u"Z");

  EXPECT_EQ(font.to_unicode(codes2({1})), "Z");
}

TEST(PdfFont, simple_font_glyph_for_code_via_cmap) {
  // A simple (1-byte) TrueType font: the code's Unicode reaches the glyph
  // through the embedded (3,1) cmap.
  Font font;
  font.embedded_font = sample_font();

  EXPECT_EQ(font.glyph_for_code('A'), 1);
  EXPECT_EQ(font.glyph_for_code('C'), 3);
}

TEST(PdfFont, no_font_yields_no_glyph) {
  Font font;
  font.composite = true;
  EXPECT_EQ(font.glyph_for_code(1), 0);
}
