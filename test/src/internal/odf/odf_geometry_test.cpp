#include <odr/internal/odf/odf_geometry.hpp>

#include <odr/document_element.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <string>
#include <string_view>

using namespace odr;
using namespace odr::internal::odf;

namespace {

void expect_linear(const DrawingTransform &transform, const double a,
                   const double b, const double c, const double d) {
  EXPECT_NEAR(a, transform.a, 1e-9);
  EXPECT_NEAR(b, transform.b, 1e-9);
  EXPECT_NEAR(c, transform.c, 1e-9);
  EXPECT_NEAR(d, transform.d, 1e-9);
}

} // namespace

TEST(OdfTransform, empty_list_is_the_identity) {
  const std::optional<DrawingTransform> transform = parse_transform("");
  ASSERT_TRUE(transform.has_value());
  expect_linear(*transform, 1, 0, 0, 1);
  EXPECT_EQ(Measure(0, DynamicUnit()), transform->e);
  EXPECT_EQ(Measure(0, DynamicUnit()), transform->f);
}

TEST(OdfTransform, translate_keeps_the_unit_it_was_written_in) {
  const std::optional<DrawingTransform> transform =
      parse_transform("translate (23.084cm 2.415cm)");
  ASSERT_TRUE(transform.has_value());
  expect_linear(*transform, 1, 0, 0, 1);
  EXPECT_EQ(Measure(23.084, DynamicUnit("cm")), transform->e);
  EXPECT_EQ(Measure(2.415, DynamicUnit("cm")), transform->f);
}

TEST(OdfTransform, translate_defaults_its_second_coordinate_to_zero) {
  const std::optional<DrawingTransform> transform =
      parse_transform("translate (4in)");
  ASSERT_TRUE(transform.has_value());
  EXPECT_EQ(Measure(4, DynamicUnit("in")), transform->e);
  EXPECT_EQ(Measure(0, DynamicUnit("in")), transform->f);
}

TEST(OdfTransform, rotate_is_counter_clockwise_and_in_radians) {
  const std::optional<DrawingTransform> transform =
      parse_transform("rotate (0.340164671213695)");
  ASSERT_TRUE(transform.has_value());
  const double angle = 0.340164671213695;
  expect_linear(*transform, std::cos(angle), -std::sin(angle), std::sin(angle),
                std::cos(angle));
}

TEST(OdfTransform, a_translate_after_a_rotate_is_not_itself_rotated) {
  const std::optional<DrawingTransform> transform = parse_transform(
      "rotate (0.340164671213695) translate (23.084cm 2.415cm)");
  ASSERT_TRUE(transform.has_value());
  EXPECT_EQ(Measure(23.084, DynamicUnit("cm")), transform->e);
  EXPECT_EQ(Measure(2.415, DynamicUnit("cm")), transform->f);
}

TEST(OdfTransform, a_rotate_after_a_translate_turns_the_translation_too) {
  const std::optional<DrawingTransform> transform =
      parse_transform("translate (1cm 0cm) rotate (1.5707963267948966)");
  ASSERT_TRUE(transform.has_value());
  // A quarter turn counter-clockwise sends (1cm, 0) to (0, -1cm).
  EXPECT_NEAR(0, transform->e.magnitude(), 1e-9);
  EXPECT_NEAR(-1, transform->f.magnitude(), 1e-9);
}

TEST(OdfTransform, matrix_is_taken_as_written) {
  const std::optional<DrawingTransform> transform =
      parse_transform("matrix (2 0 0 3 1cm 2cm)");
  ASSERT_TRUE(transform.has_value());
  expect_linear(*transform, 2, 0, 0, 3);
  EXPECT_EQ(Measure(1, DynamicUnit("cm")), transform->e);
  EXPECT_EQ(Measure(2, DynamicUnit("cm")), transform->f);
}

TEST(OdfTransform, scale_takes_one_factor_for_both_axes) {
  const std::optional<DrawingTransform> transform =
      parse_transform("scale (2)");
  ASSERT_TRUE(transform.has_value());
  expect_linear(*transform, 2, 0, 0, 2);
}

TEST(OdfTransform, lengths_that_disagree_on_a_unit_reduce_to_centimetres) {
  const std::optional<DrawingTransform> transform =
      parse_transform("translate (1in 0cm) translate (1cm 0cm)");
  ASSERT_TRUE(transform.has_value());
  EXPECT_EQ(DynamicUnit("cm"), transform->e.unit());
  EXPECT_NEAR(3.54, transform->e.magnitude(), 1e-9);
}

TEST(OdfTransform, a_skew_composes_with_the_rest) {
  const std::optional<DrawingTransform> transform =
      parse_transform("skewX (0.5) translate (1cm 0cm)");
  ASSERT_TRUE(transform.has_value());
  expect_linear(*transform, 1, 0, -std::tan(0.5), 1);
  EXPECT_EQ(Measure(1, DynamicUnit("cm")), transform->e);
}

TEST(OdfTransform, separators_may_be_commas) {
  const std::optional<DrawingTransform> transform =
      parse_transform("translate(1cm,2cm),scale(2,3)");
  ASSERT_TRUE(transform.has_value());
  expect_linear(*transform, 2, 0, 0, 3);
  EXPECT_EQ(Measure(2, DynamicUnit("cm")), transform->e);
  EXPECT_EQ(Measure(6, DynamicUnit("cm")), transform->f);
}

TEST(OdfTransform, an_unreadable_list_is_dropped_whole) {
  EXPECT_FALSE(parse_transform("rotate").has_value());
  EXPECT_FALSE(parse_transform("rotate (").has_value());
  EXPECT_FALSE(parse_transform("wobble (1)").has_value());
  EXPECT_FALSE(parse_transform("translate (1cm 2cm").has_value());
}

TEST(OdfTransform, a_length_in_a_relative_unit_is_not_a_length) {
  EXPECT_FALSE(parse_transform("translate (1em 2em)").has_value());
  EXPECT_FALSE(parse_transform("translate (50% 0%)").has_value());
  EXPECT_FALSE(parse_transform("translate (1 2)").has_value());
}

TEST(OdfTransform, a_leading_plus_is_a_sign) {
  const std::optional<DrawingTransform> transform =
      parse_transform("translate (+1cm +2cm)");
  ASSERT_TRUE(transform.has_value());
  EXPECT_EQ(Measure(1, DynamicUnit("cm")), transform->e);
  EXPECT_EQ(Measure(2, DynamicUnit("cm")), transform->f);
}

TEST(OdfTransform, an_exponent_is_a_number) {
  const std::optional<DrawingTransform> transform =
      parse_transform("skewX (-5.59465989067396E-017)");
  ASSERT_TRUE(transform.has_value());
  expect_linear(*transform, 1, 0, 5.59465989067396E-017, 1);
}

TEST(OdfTransform, the_view_ends_the_list) {
  // Nothing here reads for a terminator, so the operation the view cuts off is
  // not seen.
  const std::string_view value = "translate (1cm 2cm) translate (4cm 8cm)";
  const std::optional<DrawingTransform> transform =
      parse_transform(value.substr(0, 19));
  ASSERT_TRUE(transform.has_value());
  EXPECT_EQ(Measure(1, DynamicUnit("cm")), transform->e);
  EXPECT_EQ(Measure(2, DynamicUnit("cm")), transform->f);
}

TEST(OdfTransform, a_zero_needs_no_unit) {
  const std::optional<DrawingTransform> transform =
      parse_transform("translate (0 0)");
  ASSERT_TRUE(transform.has_value());
  EXPECT_EQ(Measure(0, DynamicUnit()), transform->e);
}
