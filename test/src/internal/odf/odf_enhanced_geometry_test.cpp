#include <odr/internal/odf/odf_enhanced_geometry.hpp>

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

using namespace odr::internal::odf;

namespace {

EnhancedGeometryContext square(std::vector<double> modifiers = {}) {
  EnhancedGeometryContext context;
  context.modifiers = std::move(modifiers);
  return context;
}

/// Resolves `?name` against a fixed table, which is all a formula sees of the
/// equations around it.
EquationResolver table(std::map<std::string, double> equations) {
  return [equations = std::move(equations)](
             const std::string_view name) -> std::optional<double> {
    const auto it = equations.find(std::string(name));
    return it == equations.end() ? std::nullopt
                                 : std::optional<double>(it->second);
  };
}

EquationResolver none() {
  return [](std::string_view) { return std::optional<double>(); };
}

std::optional<double> evaluate(const std::string &formula,
                               const EnhancedGeometryContext &context,
                               const EquationResolver &equations) {
  return evaluate_formula(formula, context, equations);
}

} // namespace

TEST(OdfFormula, arithmetic_binds_the_way_it_reads) {
  EXPECT_EQ(7, evaluate("1+2*3", square(), none()));
  EXPECT_EQ(9, evaluate("(1+2)*3", square(), none()));
  EXPECT_EQ(-5, evaluate("-1-4", square(), none()));
  EXPECT_EQ(2.5, evaluate("10/4", square(), none()));
}

TEST(OdfFormula, a_modifier_is_indexed_by_its_dollar) {
  const EnhancedGeometryContext context = square({10800, 5400});
  EXPECT_EQ(10800, evaluate("$0 ", context, none()));
  EXPECT_EQ(5400, evaluate("$1", context, none()));
  EXPECT_EQ(5400, evaluate("$0 /2", context, none()));
  EXPECT_FALSE(evaluate("$2", context, none()).has_value());
}

TEST(OdfFormula, a_reference_resolves_through_the_equations) {
  const EquationResolver equations = table({{"f0", 10800}, {"f1", 3600}});
  EXPECT_EQ(10800, evaluate("21600-?f0 ", square(), equations));
  EXPECT_EQ(14400, evaluate("?f1 +10800", square(), equations));
  EXPECT_FALSE(evaluate("?f9", square(), equations).has_value());
}

TEST(OdfFormula, the_view_box_is_named) {
  EnhancedGeometryContext context = square();
  context.left = 10;
  context.top = 20;
  context.right = 110;
  context.bottom = 220;
  context.logical_width = 5000;
  EXPECT_EQ(10, evaluate("left", context, none()));
  EXPECT_EQ(100, evaluate("width", context, none()));
  EXPECT_EQ(200, evaluate("height", context, none()));
  EXPECT_EQ(5000, evaluate("logwidth", context, none()));
}

TEST(OdfFormula, if_takes_the_second_argument_for_a_positive_first) {
  EXPECT_EQ(2, evaluate("if(1,2,3)", square(), none()));
  EXPECT_EQ(3, evaluate("if(0,2,3)", square(), none()));
  EXPECT_EQ(3, evaluate("if(-1,2,3)", square(), none()));
}

TEST(OdfFormula, the_trigonometric_functions_take_radians) {
  const std::optional<double> value =
      evaluate("sin(90*(pi/180))", square(), none());
  ASSERT_TRUE(value.has_value());
  EXPECT_NEAR(1, *value, 1e-9);
  EXPECT_EQ(5, evaluate("abs(0-5)", square(), none()));
  EXPECT_EQ(4, evaluate("sqrt(16)", square(), none()));
  EXPECT_EQ(3, evaluate("min(3,7)", square(), none()));
  EXPECT_EQ(7, evaluate("max(3,7)", square(), none()));
}

TEST(OdfFormula, an_unreadable_formula_is_dropped) {
  EXPECT_FALSE(evaluate("", square(), none()).has_value());
  EXPECT_FALSE(evaluate("1+", square(), none()).has_value());
  EXPECT_FALSE(evaluate("(1", square(), none()).has_value());
  EXPECT_FALSE(evaluate("wobble", square(), none()).has_value());
  EXPECT_FALSE(evaluate("sin(1,2)", square(), none()).has_value());
  EXPECT_FALSE(evaluate("1/0", square(), none()).has_value());
  EXPECT_FALSE(evaluate("1 2", square(), none()).has_value());
}

TEST(OdfEnhancedPath, a_line_run_repeats_its_command) {
  const std::optional<std::string> path = convert_enhanced_path(
      "M 0 0 L 21600 0 21600 21600 0 21600 Z N", square(), none());
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("M 0 0 L 21600 0 L 21600 21600 L 0 21600 Z", *path);
}

TEST(OdfEnhancedPath, a_value_may_be_a_modifier_or_a_reference) {
  const std::optional<std::string> path =
      convert_enhanced_path("M ?f0 0 L 21600 21600 0 21600 Z N",
                            square({10800}), table({{"f0", 10800}}));
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("M 10800 0 L 21600 21600 L 0 21600 Z", *path);
}

TEST(OdfEnhancedPath, a_curve_run_takes_six_values_at_a_time) {
  const std::optional<std::string> path = convert_enhanced_path(
      "M 0 0 C 1 2 3 4 5 6 7 8 9 10 11 12", square(), none());
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("M 0 0 C 1 2 3 4 5 6 C 7 8 9 10 11 12", *path);
}

TEST(OdfEnhancedPath, a_full_angle_ellipse_is_split_so_svg_can_draw_it) {
  const std::optional<std::string> path = convert_enhanced_path(
      "U 10800 10800 10800 10800 0 360 Z N", square(), none());
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("M 21600 10800 A 10800 10800 0 0 1 0 10800 "
            "A 10800 10800 0 0 1 21600 10800 Z",
            *path);
}

TEST(OdfEnhancedPath, an_angle_ellipse_to_reaches_its_start_with_a_line) {
  const std::optional<std::string> path =
      convert_enhanced_path("M 0 0 T 100 100 50 50 0 90", square(), none());
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("M 0 0 L 150 100 A 50 50 0 0 1 100 150", *path);
}

TEST(OdfEnhancedPath, a_quadrant_leaves_along_the_axis_it_names) {
  const std::optional<std::string> x =
      convert_enhanced_path("M 3590 0 X 0 3590", square(), none());
  ASSERT_TRUE(x.has_value());
  EXPECT_EQ("M 3590 0 A 3590 3590 0 0 0 0 3590", *x);

  const std::optional<std::string> y =
      convert_enhanced_path("M 0 18010 Y 3590 21600", square(), none());
  ASSERT_TRUE(y.has_value());
  EXPECT_EQ("M 0 18010 A 3590 3590 0 0 0 3590 21600", *y);
}

TEST(OdfEnhancedPath, an_arc_runs_the_way_its_command_says) {
  const std::optional<std::string> counter_clockwise =
      convert_enhanced_path("B 0 0 100 100 100 50 50 0", square(), none());
  ASSERT_TRUE(counter_clockwise.has_value());
  EXPECT_EQ("M 100 50 A 50 50 0 0 0 50 0", *counter_clockwise);

  const std::optional<std::string> clockwise =
      convert_enhanced_path("V 0 0 100 100 100 50 50 0", square(), none());
  ASSERT_TRUE(clockwise.has_value());
  // Three quarters of a turn, split in two so neither needs the large-arc flag.
  EXPECT_EQ("M 100 50 A 50 50 0 0 1 14.64466 85.35534 A 50 50 0 0 1 50 0",
            *clockwise);
}

TEST(OdfEnhancedPath, the_paint_modifiers_are_read_and_dropped) {
  const std::optional<std::string> path =
      convert_enhanced_path("F M 0 0 L 10 10 S N", square(), none());
  ASSERT_TRUE(path.has_value());
  EXPECT_EQ("M 0 0 L 10 10", *path);
}

TEST(OdfEnhancedPath, an_unreadable_path_is_dropped_whole) {
  EXPECT_FALSE(convert_enhanced_path("", square(), none()).has_value());
  EXPECT_FALSE(convert_enhanced_path("N", square(), none()).has_value());
  EXPECT_FALSE(convert_enhanced_path("M 0", square(), none()).has_value());
  EXPECT_FALSE(convert_enhanced_path("R 0 0", square(), none()).has_value());
  EXPECT_FALSE(convert_enhanced_path("M ?f0 0", square(), none()).has_value());
}
