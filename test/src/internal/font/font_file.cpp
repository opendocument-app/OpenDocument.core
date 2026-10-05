#include <odr/internal/font/font_file.hpp>

#include <odr/file.hpp>
#include <odr/html.hpp>

#include <odr/internal/abstract/font.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/font/sfnt_transform.hpp>
#include <odr/internal/magic.hpp>
#include <odr/internal/util/byte_string.hpp>

#include <internal/font/sfnt_test_util.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::font;

namespace {

using namespace odr::test::font;

namespace bs = odr::internal::util::byte_string;

std::string sample_ttf() {
  return build_sfnt(0x00010000,
                    {{"cmap", cmap_table(3, 1, cmap_format4('A', 3))},
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

  const auto font = font_file.font();
  ASSERT_NE(font, nullptr);
  EXPECT_EQ(font->name(), "TestFont");
  EXPECT_EQ(font->glyph_count(), 4);
  EXPECT_EQ(font->glyph_for_code_point('A'), 1);
}

TEST(FontFileTest, specimen_page_embeds_font_and_glyph_grid) {
  const odr::FontFile font_file(std::make_shared<font::FontFile>(
      std::make_shared<MemoryFile>(sample_ttf()), FileType::truetype_font));

  const HtmlConfig config;
  const HtmlService service = html::translate(font_file, config);

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
