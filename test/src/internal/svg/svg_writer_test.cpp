#include <odr/internal/svg/svg_writer.hpp>

#include <limits>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

using namespace odr::internal::svg;

namespace {

std::string write(void (*body)(SvgWriter &)) {
  std::ostringstream out;
  SvgWriter writer(out);
  body(writer);
  return out.str();
}

} // namespace

TEST(SvgWriter, an_element_without_content_closes_itself) {
  EXPECT_EQ("<rect x=\"1\" />", write([](SvgWriter &out) {
              out.write_element_begin("rect");
              out.write_attribute("x", 1.0);
              out.write_element_end();
            }));
}

TEST(SvgWriter, style_declarations_are_collected_into_one_attribute) {
  EXPECT_EQ("<text x=\"2\" style=\"fill:red;font-size:3;\">hi</text>",
            write([](SvgWriter &out) {
              out.write_element_begin("text");
              out.write_style("fill", "red");
              out.write_attribute("x", 2.0);
              out.write_style("font-size", 3.0);
              out.write_text("hi");
              out.write_element_end();
            }));
}

TEST(SvgWriter, a_style_value_cannot_open_another_declaration) {
  EXPECT_EQ("<text style=\"font-family:ab;fill:red;\" />",
            write([](SvgWriter &out) {
              out.write_element_begin("text");
              out.write_style("font-family", "a;b");
              out.write_style("fill", "red");
              out.write_element_end();
            }));
}

TEST(SvgWriter, a_style_value_keeps_the_semicolon_of_its_own_escape) {
  EXPECT_EQ("<text style=\"font-family:a&quot;b&amp;c;\" />",
            write([](SvgWriter &out) {
              out.write_element_begin("text");
              out.write_style("font-family", "a\"b&c");
              out.write_element_end();
            }));
}

TEST(SvgWriter, elements_nest) {
  EXPECT_EQ("<g><rect /></g>", write([](SvgWriter &out) {
              out.write_element_begin("g");
              out.write_element_begin("rect");
              out.write_element_end();
              out.write_element_end();
            }));
}

TEST(SvgWriter, format_number) {
  EXPECT_EQ("0", format_number(0));
  EXPECT_EQ("1", format_number(1));
  EXPECT_EQ("-1.5", format_number(-1.5));
  EXPECT_EQ("10519.4", format_number(10519.375));
}

TEST(SvgWriter, format_number_never_uses_an_exponent) {
  EXPECT_EQ("12345678", format_number(12345678.0));
  EXPECT_EQ("0.0000001", format_number(0.0000001));
}

TEST(SvgWriter, format_number_of_something_that_is_not_a_number) {
  EXPECT_EQ("0", format_number(std::numeric_limits<double>::quiet_NaN()));
  EXPECT_EQ("0", format_number(std::numeric_limits<double>::infinity()));
}
