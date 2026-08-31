#include <odr/internal/odf/odf_geometry.hpp>

#include <odr/document_element.hpp>

#include <pugixml.hpp>

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

namespace {

pugi::xml_node parse_shape(pugi::xml_document &document,
                           const std::string &xml) {
  EXPECT_TRUE(document.load_string(xml.c_str()));
  return document.first_child();
}

} // namespace

TEST(OdfPath, commands_are_kept_and_numbers_re_rendered) {
  const std::optional<DrawingPath> path =
      parse_path_data("M10,20L30,40 50,60Z");
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("M 10 20 L 30 40 L 50 60 Z", path->data);
}

TEST(OdfPath, a_repeated_moveto_pair_is_a_lineto) {
  const std::optional<DrawingPath> path = parse_path_data("m0 0 10 10 20 0");
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("m 0 0 l 10 10 l 20 0", path->data);
}

TEST(OdfPath, the_box_covers_every_point_and_control_point) {
  const std::optional<DrawingPath> path =
      parse_path_data("M 0 0 C 10 -20 30 40 20 0");
  ASSERT_TRUE(path.has_value());
  EXPECT_DOUBLE_EQ(0, path->x);
  EXPECT_DOUBLE_EQ(-20, path->y);
  EXPECT_DOUBLE_EQ(30, path->width);
  EXPECT_DOUBLE_EQ(60, path->height);
}

TEST(OdfPath, a_relative_command_is_boxed_where_it_lands) {
  const std::optional<DrawingPath> path =
      parse_path_data("m 100 100 h 50 v 25");
  ASSERT_TRUE(path.has_value());
  EXPECT_DOUBLE_EQ(100, path->x);
  EXPECT_DOUBLE_EQ(100, path->y);
  EXPECT_DOUBLE_EQ(50, path->width);
  EXPECT_DOUBLE_EQ(25, path->height);
}

TEST(OdfPath, an_unreadable_path_is_dropped_whole) {
  EXPECT_FALSE(parse_path_data("").has_value());
  EXPECT_FALSE(parse_path_data("10 20").has_value());
  EXPECT_FALSE(parse_path_data("M 10").has_value());
  EXPECT_FALSE(parse_path_data("M 0 0 W 1 2").has_value());
}

TEST(OdfShape, draw_path_is_written_in_its_view_box) {
  pugi::xml_document document;
  const std::optional<DrawingPath> path = read_path(parse_shape(
      document,
      R"(<draw:path svg:viewBox="0 0 100 200" svg:d="M0 0L100 200Z"/>)"));
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("M 0 0 L 100 200 Z", path->data);
  EXPECT_DOUBLE_EQ(0, path->x);
  EXPECT_DOUBLE_EQ(100, path->width);
  EXPECT_DOUBLE_EQ(200, path->height);
}

TEST(OdfShape, a_polygon_closes_and_a_polyline_does_not) {
  pugi::xml_document polygon;
  const std::optional<DrawingPath> closed =
      read_path(parse_shape(polygon, R"(<draw:polygon svg:viewBox="0 0 10 10" )"
                                     R"(draw:points="0,0 10,0 10,10"/>)"));
  ASSERT_TRUE(closed.has_value());
  EXPECT_EQ("M 0 0 L 10 0 L 10 10 Z", closed->data);

  pugi::xml_document polyline;
  const std::optional<DrawingPath> open = read_path(
      parse_shape(polyline, R"(<draw:polyline svg:viewBox="0 0 10 10" )"
                            R"(draw:points="0,0 10,0 10,10"/>)"));
  ASSERT_TRUE(open.has_value());
  EXPECT_EQ("M 0 0 L 10 0 L 10 10", open->data);
}

TEST(OdfShape, a_regular_polygon_starts_at_the_top) {
  pugi::xml_document document;
  const std::optional<DrawingPath> path = read_path(
      parse_shape(document, R"(<draw:regular-polygon draw:corners="4"/>)"));
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("M 10800 0 L 21600 10800 L 10800 21600 L 0 10800 Z", path->data);
  EXPECT_DOUBLE_EQ(21600, path->width);
}

TEST(OdfShape, a_connector_is_boxed_by_its_own_path) {
  pugi::xml_document document;
  const std::optional<DrawingPath> path = read_path(parse_shape(
      document, R"(<draw:connector svg:x1="4.4cm" svg:y1="11.4cm" )"
                R"(svg:d="m4400 11400c1050 0 1575 -1666 1575 -5000"/>)"));
  ASSERT_TRUE(path.has_value());
  EXPECT_DOUBLE_EQ(4400, path->x);
  EXPECT_DOUBLE_EQ(6400, path->y);
  EXPECT_DOUBLE_EQ(1575, path->width);
  EXPECT_DOUBLE_EQ(5000, path->height);
}

TEST(OdfShape, a_straight_connector_keeps_a_box_svg_accepts) {
  pugi::xml_document document;
  const std::optional<DrawingPath> path = read_path(parse_shape(
      document, R"(<draw:connector svg:d="M 100 100 L 500 100"/>)"));
  ASSERT_TRUE(path.has_value());
  EXPECT_DOUBLE_EQ(400, path->width);
  EXPECT_DOUBLE_EQ(1, path->height);
}

TEST(OdfShape, a_full_ellipse_has_no_path_of_its_own) {
  pugi::xml_document document;
  EXPECT_FALSE(
      read_path(parse_shape(document, R"(<draw:ellipse svg:width="1cm"/>)"))
          .has_value());
  pugi::xml_document full;
  EXPECT_FALSE(
      read_path(parse_shape(full, R"(<draw:ellipse draw:kind="full"/>)"))
          .has_value());
}

TEST(OdfShape, an_elliptical_arc_traces_its_angles_counter_clockwise) {
  pugi::xml_document document;
  const std::optional<DrawingPath> path = read_path(
      parse_shape(document, R"(<draw:ellipse draw:kind="arc" )"
                            R"(draw:start-angle="0" draw:end-angle="90"/>)"));
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("M 21600 10800 A 10800 10800 0 0 0 10800 0", path->data);
}

TEST(OdfShape, a_section_closes_through_the_centre_and_a_cut_across_it) {
  pugi::xml_document section_document;
  const std::optional<DrawingPath> section = read_path(parse_shape(
      section_document, R"(<draw:circle draw:kind="section" )"
                        R"(draw:start-angle="0" draw:end-angle="90"/>)"));
  ASSERT_TRUE(section.has_value());
  EXPECT_TRUE(section->data.ends_with("L 10800 10800 Z"));

  pugi::xml_document cut_document;
  const std::optional<DrawingPath> cut = read_path(parse_shape(
      cut_document, R"(<draw:circle draw:kind="cut" )"
                    R"(draw:start-angle="0" draw:end-angle="90"/>)"));
  ASSERT_TRUE(cut.has_value());
  EXPECT_TRUE(cut->data.ends_with("10800 0 Z"));
}

TEST(OdfShape, an_arc_over_half_the_ellipse_sets_the_large_arc_flag) {
  pugi::xml_document document;
  const std::optional<DrawingPath> path = read_path(
      parse_shape(document, R"(<draw:ellipse draw:kind="arc" )"
                            R"(draw:start-angle="0" draw:end-angle="270"/>)"));
  ASSERT_TRUE(path.has_value());
  EXPECT_NE(std::string::npos, path->data.find(" 0 1 0 "));
}
