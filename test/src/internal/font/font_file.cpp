#include <odr/internal/font/font_file.hpp>

#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/font.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/font/otf_writer.hpp>
#include <odr/internal/magic.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::internal;
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

std::string head_table() {
  std::string t(54, '\0');
  t[18] = 0x03; // unitsPerEm = 1000
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

std::string hhea_table(std::uint16_t metrics) {
  std::string t(36, '\0');
  t[34] = static_cast<char>(metrics >> 8);
  t[35] = static_cast<char>(metrics & 0xff);
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

std::string cmap_table() {
  std::string sub;
  put16(sub, 4);
  put16(sub, 32);
  put16(sub, 0);
  put16(sub, 4);
  put16(sub, 0);
  put16(sub, 0);
  put16(sub, 0);
  put16(sub, 'C');    // endCode[0]
  put16(sub, 0xffff); // endCode[1]
  put16(sub, 0);
  put16(sub, 'A');                                 // startCode[0]
  put16(sub, 0xffff);                              // startCode[1]
  put16(sub, static_cast<std::uint16_t>(1 - 'A')); // idDelta[0]: A->1,B->2,C->3
  put16(sub, 1);                                   // idDelta[1]
  put16(sub, 0);
  put16(sub, 0);

  std::string t;
  put16(t, 0);
  put16(t, 1);
  put16(t, 3);
  put16(t, 1);
  put32(t, 12);
  t += sub;
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

std::string sample_ttf() {
  return build_sfnt(0x00010000, {{"cmap", cmap_table()},
                                 {"head", head_table()},
                                 {"hhea", hhea_table(4)},
                                 {"hmtx", hmtx_table({500, 600, 700, 800})},
                                 {"maxp", maxp_table(4)},
                                 {"name", name_table("TestFont")}});
}

} // namespace

TEST(FontFileTest, magic_detects_sfnt_flavors) {
  EXPECT_EQ(magic::file_type(sample_ttf()), FileType::truetype_font);
  EXPECT_EQ(magic::file_type(std::string("OTTO\0\0\0\0\0\0\0\0", 12)),
            FileType::opentype_font);
  EXPECT_EQ(magic::file_type(std::string("ttcf\0\0\0\0\0\0\0\0", 12)),
            FileType::truetype_font);
}

TEST(FontFileTest, font_file_facts) {
  const font::FontFile font_file(std::make_shared<MemoryFile>(sample_ttf()),
                                 FileType::truetype_font);

  EXPECT_EQ(font_file.file_type(), FileType::truetype_font);
  EXPECT_EQ(font_file.file_category(), FileCategory::font);
  EXPECT_TRUE(font_file.is_decodable());

  const auto program = font_file.font_program();
  ASSERT_NE(program, nullptr);
  EXPECT_EQ(program->name(), "TestFont");
  EXPECT_EQ(program->glyph_count(), 4);
  EXPECT_EQ(program->glyph_for_code_point('A'), 1);
}

TEST(FontFileTest, specimen_page_embeds_font_and_glyph_grid) {
  const odr::FontFile font_file(std::make_shared<font::FontFile>(
      std::make_shared<MemoryFile>(sample_ttf()), FileType::truetype_font));

  const std::string cache_path =
      (std::filesystem::temp_directory_path() / "odr_font_test").string();
  const HtmlConfig config;
  const HtmlService service = html::translate(font_file, cache_path, config);

  const HtmlViews &views = service.list_views();
  ASSERT_EQ(views.size(), 1);
  EXPECT_EQ(views.at(0).name(), "font");

  std::ostringstream out;
  service.write_html("font.html", out);
  const std::string html = out.str();

  EXPECT_NE(html.find("@font-face"), std::string::npos);
  EXPECT_NE(html.find("odr-specimen"), std::string::npos);
  EXPECT_NE(html.find("TestFont"), std::string::npos);
  // Glyph 1 ('A') is shown at its PUA code point with its recovered Unicode.
  EXPECT_NE(html.find("&#xe001;"), std::string::npos);
  EXPECT_NE(html.find("U+41"), std::string::npos);
}
