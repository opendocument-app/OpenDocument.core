#include <odr/document_element.hpp>
#include <odr/quantity.hpp>
#include <odr/style.hpp>

#include <odr/internal/html/document_style.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>

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

TEST(html_document_style, text_style_writes_one_line) {
  TextStyle text_style;
  text_style.font_underline = true;
  EXPECT_EQ(ihtml::translate_text_style(text_style),
            "text-decoration:underline;");

  text_style.font_underline = std::nullopt;
  text_style.font_line_through = true;
  EXPECT_EQ(ihtml::translate_text_style(text_style),
            "text-decoration:line-through;");
}

TEST(html_document_style, text_style_writes_both_lines_in_one_declaration) {
  TextStyle text_style;
  text_style.font_underline = true;
  text_style.font_line_through = true;
  EXPECT_EQ(ihtml::translate_text_style(text_style),
            "text-decoration:underline line-through;");
}

TEST(html_document_style, text_style_writes_no_line_for_one_turned_off) {
  TextStyle text_style;
  text_style.font_underline = false;
  text_style.font_line_through = false;
  EXPECT_EQ(ihtml::translate_text_style(text_style), "");
}

TEST(html_document_style, block_font_style_carries_the_font) {
  TextStyle text_style;
  text_style.font_name = "Arial";
  text_style.font_size = Measure("14pt");
  EXPECT_EQ(ihtml::translate_block_font_style(text_style),
            "font-family:'Arial',sans-serif;font-size:14pt;");
}

TEST(html_document_style, font_family_quotes_a_name_and_adds_its_kind) {
  EXPECT_EQ(ihtml::translate_font_family("Calibri"), "'Calibri',sans-serif");
  EXPECT_EQ(ihtml::translate_font_family("Times New Roman"),
            "'Times New Roman',serif");
  EXPECT_EQ(ihtml::translate_font_family("Courier New"),
            "'Courier New',monospace");
  EXPECT_EQ(ihtml::translate_font_family("PT Serif"), "'PT Serif',serif");
  EXPECT_EQ(ihtml::translate_font_family("DejaVu Sans"),
            "'DejaVu Sans',sans-serif");
  EXPECT_EQ(ihtml::translate_font_family("8514oem"), "'8514oem',sans-serif");
  EXPECT_EQ(ihtml::translate_font_family("RobotoMono-Regular"),
            "'RobotoMono-Regular',monospace");
}

// Markdown names its code `monospace`: quoted, it is a font no one has.
TEST(html_document_style, font_family_keeps_a_generic_family_a_keyword) {
  EXPECT_EQ(ihtml::translate_font_family("monospace"), "monospace");
  EXPECT_EQ(ihtml::translate_font_family(" serif "), "serif");
  EXPECT_EQ(ihtml::translate_font_family("Sans-Serif"), "Sans-Serif");
}

TEST(html_document_style, font_family_does_not_read_a_kind_into_a_longer_word) {
  EXPECT_EQ(ihtml::translate_font_family("Monotype Corsiva"),
            "'Monotype Corsiva',sans-serif");
  EXPECT_EQ(ihtml::translate_font_family("Century Gothic"),
            "'Century Gothic',sans-serif");
  EXPECT_EQ(ihtml::translate_font_family("Century Schoolbook"),
            "'Century Schoolbook',serif");
  EXPECT_EQ(ihtml::translate_font_family("Microsoft Sans Serif"),
            "'Microsoft Sans Serif',sans-serif");
}

TEST(html_document_style, font_family_escapes_what_ends_a_css_string) {
  EXPECT_EQ(ihtml::translate_font_family("O'Reilly"),
            "'O\\'Reilly',sans-serif");
  EXPECT_EQ(ihtml::translate_font_family("a\\b"), "'a\\\\b',sans-serif");
  EXPECT_EQ(ihtml::translate_font_family("a\nb"), "'a\\a b',sans-serif");
  EXPECT_EQ(ihtml::translate_font_family("'open"), "'\\'open',sans-serif");
}

TEST(html_document_style, font_family_keeps_a_list_and_its_generic) {
  EXPECT_EQ(ihtml::translate_font_family("'Liberation Serif'"),
            "'Liberation Serif',serif");
  EXPECT_EQ(ihtml::translate_font_family("'Open Sans', Arial"),
            "'Open Sans', Arial,sans-serif");
  EXPECT_EQ(ihtml::translate_font_family("'Liberation Mono', monospace"),
            "'Liberation Mono', monospace");
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

TEST(html_document_style, drawing_style_draws_a_zero_width_as_a_hairline) {
  GraphicStyle style;
  style.stroke_width = Measure("0cm");
  style.stroke_color = Color(128, 128, 128);
  EXPECT_EQ(ihtml::translate_drawing_style(style),
            "stroke-width:1px;stroke:#808080;");
}

TEST(html_document_style, drawing_style_writes_none_for_a_stroke_turned_off) {
  GraphicStyle style;
  style.stroke_width = Measure("0.1cm");
  style.stroke_color = Color(128, 128, 128, 0);
  EXPECT_EQ(ihtml::translate_drawing_style(style),
            "stroke-width:0.1cm;stroke:none;");
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

TEST(html_document_style, fill_image_is_empty_for_a_solid_fill) {
  GraphicStyle style;
  style.fill_color = Color(0, 255, 0);
  EXPECT_EQ(ihtml::translate_fill_image(style), "");
}

TEST(html_document_style, fill_image_turns_a_linear_gradient_clockwise) {
  // [OpenDocument] turns 30° counterclockwise from top-to-bottom; css turns
  // clockwise from bottom-to-top.
  GraphicStyle style;
  style.fill_gradient = GradientFill{.style = GradientStyle::linear,
                                     .start_color = Color(255, 0, 0),
                                     .end_color = Color(0, 0, 255),
                                     .angle = 30,
                                     .border = 0.2};
  EXPECT_EQ(ihtml::translate_fill_image(style),
            "background-image:linear-gradient(150deg,#ff0000 20%,#0000ff "
            "100%);");
}

TEST(html_document_style, fill_image_mirrors_an_axial_gradient) {
  GraphicStyle style;
  style.fill_gradient = GradientFill{.style = GradientStyle::axial,
                                     .start_color = Color(255, 0, 0),
                                     .end_color = Color(0, 0, 255)};
  EXPECT_EQ(ihtml::translate_fill_image(style),
            "background-image:linear-gradient(180deg,#0000ff 0%,#ff0000 "
            "50%,#0000ff 100%);");
}

TEST(html_document_style, fill_image_without_a_size_draws_radial_as_ellipse) {
  GraphicStyle style;
  style.fill_gradient = GradientFill{.style = GradientStyle::radial,
                                     .start_color = Color(128, 0, 0),
                                     .end_color = Color(255, 255, 0),
                                     .center_x = 0.3,
                                     .center_y = 0.3};
  EXPECT_EQ(ihtml::translate_fill_image(style),
            "background-image:radial-gradient(70.711% 70.711% at 30% "
            "30%,#ffff00 0%,#800000 100%);");
}

TEST(html_document_style, fill_image_draws_a_radial_gradient_as_a_circle) {
  GraphicStyle style;
  style.fill_gradient = GradientFill{.style = GradientStyle::radial,
                                     .start_color = Color(128, 0, 0),
                                     .end_color = Color(255, 255, 0),
                                     .center_x = 0.5,
                                     .center_y = 0.5};
  // half the diagonal of a 300 x 400 px box
  EXPECT_EQ(
      ihtml::translate_fill_image(style, Measure("300px"), Measure("400px")),
      "background-image:radial-gradient(circle 250px at 50% "
      "50%,#ffff00 0%,#800000 100%);");
}

TEST(html_document_style, fill_image_draws_a_crossed_hatch_as_two_patterns) {
  GraphicStyle style;
  style.fill_hatch = HatchFill{.style = HatchStyle::crossed,
                               .color = Color(0, 0, 0),
                               .distance = Measure("0.1cm"),
                               .rotation = 45};
  const std::string image = ihtml::translate_fill_image(style);
  const std::string_view prefix = "background-image:url('data:image/svg+xml,";
  EXPECT_TRUE(image.starts_with(prefix));
  // percent-encoded: nothing in it ends the url or the attribute, up to the
  // closing `');`
  EXPECT_EQ(image.find_first_of("\"<>#'", prefix.size()), image.size() - 3);
  EXPECT_NE(image.find("rotate%28-45%29"), std::string::npos);
  EXPECT_NE(image.find("rotate%28-135%29"), std::string::npos);
  EXPECT_EQ(image.find("rotate%28-90%29"), std::string::npos);
}

TEST(html_document_style, fill_image_turns_the_third_hatch_back_by_45) {
  GraphicStyle style;
  style.fill_hatch = HatchFill{.style = HatchStyle::triple,
                               .color = Color(0, 0, 0),
                               .distance = Measure("0.1cm"),
                               .rotation = 0};
  const std::string image = ihtml::translate_fill_image(style);
  EXPECT_NE(image.find("rotate%28-90%29"), std::string::npos);
  EXPECT_NE(image.find("rotate%2845%29"), std::string::npos);
}

TEST(html_document_style, fill_image_holds_a_hatch_to_two_pixels) {
  GraphicStyle style;
  style.fill_hatch =
      HatchFill{.color = Color(0, 0, 0), .distance = Measure("0.00001cm")};
  EXPECT_NE(ihtml::translate_fill_image(style).find("width=%222px%22"),
            std::string::npos);
}

TEST(html_document_style, fill_mask_scales_the_shape_to_the_box) {
  const std::string mask = ihtml::translate_fill_mask(
      "0 0 2 2", R"(<ellipse cx="1" cy="1" rx="1" ry="1"/>)");
  EXPECT_NE(mask.find("-webkit-mask-image:url('data:image/svg+xml,"),
            std::string::npos);
  EXPECT_NE(mask.find("preserveAspectRatio=%22none%22"), std::string::npos);
  EXPECT_NE(mask.find("mask-size:100% 100%;"), std::string::npos);
}
