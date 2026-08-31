#include <odr/document_element.hpp>
#include <odr/quantity.hpp>
#include <odr/style.hpp>

#include <odr/internal/html/document_style.hpp>

#include <gtest/gtest.h>

#include <optional>

using namespace odr;
namespace ihtml = odr::internal::html;

namespace {

PageLayout a4_page_layout() {
  PageLayout page_layout;
  page_layout.width = Measure("21cm");
  page_layout.height = Measure("29.7cm");
  return page_layout;
}

DrawingTransform identity() {
  return DrawingTransform{.a = 1,
                          .b = 0,
                          .c = 0,
                          .d = 1,
                          .e = Measure(0, DynamicUnit()),
                          .f = Measure(0, DynamicUnit())};
}

/// A quarter turn counter-clockwise, the shape of every rotation the corpus
/// carries.
DrawingTransform quarter_turn() {
  DrawingTransform transform = identity();
  transform.a = 0;
  transform.b = 1;
  transform.c = -1;
  transform.d = 0;
  return transform;
}

} // namespace

TEST(html_document_style, outer_page_style_fixes_both_dimensions) {
  EXPECT_EQ(ihtml::translate_outer_page_style(a4_page_layout()),
            "width:21cm;height:29.7cm;");
}

TEST(html_document_style, outer_page_style_paints_the_ground) {
  PageLayout page_layout = a4_page_layout();
  page_layout.background_color = Color(0x12, 0x34, 0x56);
  EXPECT_EQ(ihtml::translate_outer_page_style(page_layout),
            "width:21cm;height:29.7cm;background-color:#123456;");
}

TEST(html_document_style, outer_flowing_page_style_floors_the_height) {
  EXPECT_EQ(ihtml::translate_outer_flowing_page_style(a4_page_layout()),
            "width:21cm;min-height:29.7cm;");
}

TEST(html_document_style, outer_flowing_page_style_paints_the_ground) {
  PageLayout page_layout = a4_page_layout();
  page_layout.background_color = Color(0x12, 0x34, 0x56);
  EXPECT_EQ(ihtml::translate_outer_flowing_page_style(page_layout),
            "width:21cm;background-color:#123456;min-height:29.7cm;");
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

TEST(html_document_style, drawing_transform_of_nothing_is_nothing) {
  EXPECT_EQ(ihtml::translate_drawing_transform(std::nullopt), "");
}

TEST(html_document_style, drawing_transform_of_the_identity_is_nothing) {
  EXPECT_EQ(ihtml::translate_drawing_transform(identity()), "");
}

TEST(html_document_style, drawing_transform_of_a_translation_writes_no_matrix) {
  DrawingTransform transform = identity();
  transform.e = Measure(1, DynamicUnit("cm"));
  transform.f = Measure(2, DynamicUnit("cm"));
  EXPECT_EQ(ihtml::translate_drawing_transform(transform),
            "transform:translate(1cm,2cm);transform-origin:0 0;");
}

TEST(html_document_style, drawing_transform_of_a_rotation_writes_no_translate) {
  EXPECT_EQ(ihtml::translate_drawing_transform(quarter_turn()),
            "transform:matrix(0,1,-1,0,0,0);transform-origin:0 0;");
}

TEST(html_document_style, drawing_transform_writes_the_linear_part_last) {
  // Css applies the list right to left, so the shape turns about its own
  // origin and is moved after, which is what the composed matrix means.
  DrawingTransform transform = quarter_turn();
  transform.e = Measure(1, DynamicUnit("cm"));
  transform.f = Measure(2, DynamicUnit("cm"));
  EXPECT_EQ(ihtml::translate_drawing_transform(transform),
            "transform:translate(1cm,2cm) matrix(0,1,-1,0,0,0);"
            "transform-origin:0 0;");
}
