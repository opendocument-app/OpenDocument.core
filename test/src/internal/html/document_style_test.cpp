#include <odr/quantity.hpp>
#include <odr/style.hpp>

#include <odr/internal/html/document_style.hpp>

#include <gtest/gtest.h>

using namespace odr;
namespace ihtml = odr::internal::html;

namespace {

PageLayout a4_page_layout() {
  PageLayout page_layout;
  page_layout.width = Measure("21cm");
  page_layout.height = Measure("29.7cm");
  return page_layout;
}

} // namespace

TEST(html_document_style, outer_page_style_fixes_both_dimensions) {
  EXPECT_EQ(ihtml::translate_outer_page_style(a4_page_layout()),
            "width:21cm;height:29.7cm;");
}

TEST(html_document_style, outer_flowing_page_style_floors_the_height) {
  EXPECT_EQ(ihtml::translate_outer_flowing_page_style(a4_page_layout()),
            "width:21cm;min-height:29.7cm;");
}

TEST(html_document_style, outer_flowing_page_style_without_height) {
  PageLayout page_layout = a4_page_layout();
  page_layout.height = {};
  EXPECT_EQ(ihtml::translate_outer_flowing_page_style(page_layout),
            "width:21cm;");
}

TEST(html_document_style, block_font_style_carries_the_font) {
  TextStyle text_style;
  text_style.font_name = "Arial";
  text_style.font_size = Measure("14pt");
  EXPECT_EQ(ihtml::translate_block_font_style(text_style),
            "font-family:Arial;font-size:14pt;");
}

TEST(html_document_style, block_font_style_leaves_what_paints_to_the_run) {
  TextStyle text_style;
  text_style.font_size = Measure("14pt");
  text_style.font_weight = FontWeight::bold;
  text_style.background_color = Color(0xff, 0xff, 0x00);
  text_style.font_underline = true;
  EXPECT_EQ(ihtml::translate_block_font_style(text_style), "font-size:14pt;");
}

TEST(html_document_style, block_font_style_of_a_style_naming_no_font) {
  EXPECT_EQ(ihtml::translate_block_font_style(TextStyle()), "");
}
