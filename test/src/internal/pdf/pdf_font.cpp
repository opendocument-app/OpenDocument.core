#include <odr/internal/abstract/font.hpp>
#include <odr/internal/font/sfnt_font.hpp>
#include <odr/internal/pdf/pdf_afm.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_encoding.hpp>
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

TEST(PdfAfm, resolve_substitute_by_name) {
  using pdf::resolve_font_substitute;
  using pdf::StandardFont;

  const pdf::FontSubstitute helv =
      resolve_font_substitute("Helvetica", 0, 0, 0);
  EXPECT_EQ(helv.metrics, StandardFont::helvetica);
  EXPECT_FALSE(helv.bold);
  EXPECT_FALSE(helv.italic);
  EXPECT_NE(helv.css_family.find("sans-serif"), std::string::npos);

  const pdf::FontSubstitute tbi =
      resolve_font_substitute("Times-BoldItalic", 0, 0, 0);
  EXPECT_EQ(tbi.metrics, StandardFont::times_bold_italic);
  EXPECT_TRUE(tbi.bold);
  EXPECT_TRUE(tbi.italic);
  EXPECT_NE(tbi.css_family.find("serif"), std::string::npos);

  // A subset prefix (`ABCDEF+`) is stripped before matching.
  const pdf::FontSubstitute sub =
      resolve_font_substitute("ABCDEF+Courier-Bold", 0, 0, 0);
  EXPECT_EQ(sub.metrics, StandardFont::courier_bold);
  EXPECT_TRUE(sub.bold);
  EXPECT_NE(sub.css_family.find("monospace"), std::string::npos);
}

TEST(PdfAfm, resolve_substitute_by_flags) {
  using pdf::resolve_font_substitute;
  using pdf::StandardFont;

  // No recognizable name: the descriptor flags pick the family (serif bit 2,
  // fixed-pitch bit 1).
  EXPECT_EQ(resolve_font_substitute("CustomFont", 1U << 1, 0, 0).metrics,
            StandardFont::times_roman);
  EXPECT_EQ(resolve_font_substitute("CustomFont", 1U << 0, 0, 0).metrics,
            StandardFont::courier);

  // Italic flag (bit 7) + force-bold flag (bit 19).
  const pdf::FontSubstitute flagged =
      resolve_font_substitute("CustomFont", (1U << 6) | (1U << 18), 0, 0);
  EXPECT_TRUE(flagged.bold);
  EXPECT_TRUE(flagged.italic);
  EXPECT_EQ(flagged.metrics, StandardFont::helvetica_bold_oblique);

  // FontWeight >= 600 and a non-zero ItalicAngle also imply bold/italic.
  const pdf::FontSubstitute weighted =
      resolve_font_substitute("CustomFont", 0, 700, -12.0);
  EXPECT_TRUE(weighted.bold);
  EXPECT_TRUE(weighted.italic);
}

TEST(PdfAfm, glyph_and_code_widths) {
  using pdf::afm_code_width;
  using pdf::afm_width;
  using pdf::StandardFont;

  EXPECT_EQ(afm_width(StandardFont::helvetica, "A"), 667.0);
  EXPECT_EQ(afm_width(StandardFont::helvetica, "space"), 278.0);
  EXPECT_EQ(afm_width(StandardFont::times_roman, "A"), 722.0);
  EXPECT_EQ(afm_width(StandardFont::courier, "A"), 600.0);
  EXPECT_FALSE(afm_width(StandardFont::helvetica, "nonexistent"));

  // Built-in-encoding fallback: Symbol code 32 is space = 250; an empty slot
  // (code 0) has no width.
  EXPECT_EQ(afm_code_width(StandardFont::symbol, 32), 250.0);
  EXPECT_FALSE(afm_code_width(StandardFont::symbol, 0));
}

TEST(PdfFont, advance_width_afm_fallback_via_encoding) {
  // A non-embedded Helvetica with no /Widths: the advance comes from the AFM
  // table via the /Encoding glyph name.
  Font font;
  font.substitute = pdf::resolve_font_substitute("Helvetica", 0, 0, 0);
  font.encoding.emplace(pdf::BaseEncoding::standard);

  EXPECT_NEAR(font.advance_width('A'), 0.667, 1e-9);
  EXPECT_NEAR(font.advance_width(' '), 0.278, 1e-9);
}

TEST(PdfFont, advance_width_afm_fallback_builtin_encoding) {
  // No /Encoding: fall back to the substitute font's built-in code->width
  // table (the Symbol / ZapfDingbats case).
  Font font;
  font.substitute = pdf::resolve_font_substitute("Symbol", 1U << 2, 0, 0);

  EXPECT_NEAR(font.advance_width(32), 0.25, 1e-9);
}

TEST(PdfFont, advance_width_explicit_widths_win_over_afm) {
  // An explicit /Widths entry takes precedence; a code outside its range still
  // falls back to the AFM table.
  Font font;
  font.substitute = pdf::resolve_font_substitute("Helvetica", 0, 0, 0);
  font.encoding.emplace(pdf::BaseEncoding::standard);
  font.first_char = 'A';
  font.widths = {1000.0};

  EXPECT_NEAR(font.advance_width('A'), 1.0, 1e-9);   // from /Widths
  EXPECT_NEAR(font.advance_width('B'), 0.667, 1e-9); // out of range -> AFM
}

TEST(PdfFont, advance_width_explicit_encoding_miss_uses_missing_width) {
  // An explicit /Encoding names a glyph absent from the AFM table. The built-in
  // code->width table assumes the font's own encoding, which the /Differences
  // override replaces, so it must not be consulted — the advance is
  // /MissingWidth, not the width of whatever glyph sits at the same code.
  Font font;
  font.substitute = pdf::resolve_font_substitute("Helvetica", 0, 0, 0);
  font.encoding.emplace(pdf::BaseEncoding::standard);
  font.encoding->set_difference('C', "customglyph"); // code 67, AFM 'C' = 722
  font.missing_width = 250.0;

  EXPECT_NEAR(font.advance_width('C'), 0.25, 1e-9);
}
