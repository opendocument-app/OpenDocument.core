#include <odr/internal/pdf/pdf_shading.hpp>

#include <odr/internal/pdf/pdf_color.hpp>
#include <odr/internal/pdf/pdf_object.hpp>

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

namespace {

// A context with no indirection: objects resolve to themselves and no stream is
// needed (the shadings under test use exponential tint functions).
ShadingContext context() {
  ShadingContext ctx;
  ctx.resolve = [](const Object &object) { return object; };
  ctx.load_stream = [](const Object &) { return std::string{}; };
  return ctx;
}

// DeviceRGB resolves without a named lookup; the colour-space resolver is
// inert.
ColorSpaceContext color_context() {
  ColorSpaceContext ctx;
  ctx.resolve = [](const Object &object) { return object; };
  ctx.load_stream = [](const Object &) { return std::string{}; };
  ctx.named = nullptr;
  return ctx;
}

Object reals(std::initializer_list<double> values) {
  std::vector<Object> holder;
  for (const double value : values) {
    holder.emplace_back(Real{value});
  }
  return Object(Array(std::move(holder)));
}

// A linear DeviceRGB tint from C0 to C1 (type 2, N = 1).
Object linear_function(std::initializer_list<double> c0,
                       std::initializer_list<double> c1) {
  Dictionary fn;
  fn["FunctionType"] = Object(Integer{2});
  fn["Domain"] = reals({0, 1});
  fn["C0"] = reals(c0);
  fn["C1"] = reals(c1);
  fn["N"] = Object(Real{1});
  return Object(fn);
}

} // namespace

// Type 2 (axial): the tint function is sampled into stops spanning black to
// white, with the axis coordinates carried verbatim.
TEST(PdfShading, axial_basic) {
  Dictionary dict;
  dict["ShadingType"] = Object(Integer{2});
  dict["ColorSpace"] = Object(Name{"DeviceRGB"});
  dict["Coords"] = reals({10, 20, 110, 20});
  dict["Function"] = linear_function({0, 0, 0}, {1, 1, 1});

  const auto shading = parse_shading(Object(dict), context(), color_context());
  ASSERT_NE(shading, nullptr);
  EXPECT_EQ(shading->type, 2);
  EXPECT_DOUBLE_EQ(shading->coords[0], 10);
  EXPECT_DOUBLE_EQ(shading->coords[1], 20);
  EXPECT_DOUBLE_EQ(shading->coords[2], 110);
  EXPECT_DOUBLE_EQ(shading->coords[3], 20);
  ASSERT_GE(shading->stops.size(), 2);
  EXPECT_DOUBLE_EQ(shading->stops.front().offset, 0.0);
  EXPECT_DOUBLE_EQ(shading->stops.back().offset, 1.0);
  EXPECT_NEAR(shading->stops.front().rgb[0], 0.0, 1e-6);
  EXPECT_NEAR(shading->stops.back().rgb[0], 1.0, 1e-6);
}

// Type 3 (radial): both circles' six coordinates are taken.
TEST(PdfShading, radial_basic) {
  Dictionary dict;
  dict["ShadingType"] = Object(Integer{3});
  dict["ColorSpace"] = Object(Name{"DeviceRGB"});
  dict["Coords"] = reals({0, 0, 0, 0, 0, 50});
  dict["Function"] = linear_function({1, 0, 0}, {0, 0, 1});

  const auto shading = parse_shading(Object(dict), context(), color_context());
  ASSERT_NE(shading, nullptr);
  EXPECT_EQ(shading->type, 3);
  EXPECT_DOUBLE_EQ(shading->coords[2], 0);  // inner radius
  EXPECT_DOUBLE_EQ(shading->coords[5], 50); // outer radius
}

// /Domain and /Extend are parsed; defaults apply when absent.
TEST(PdfShading, domain_and_extend) {
  Dictionary dict;
  dict["ShadingType"] = Object(Integer{2});
  dict["ColorSpace"] = Object(Name{"DeviceRGB"});
  dict["Coords"] = reals({0, 0, 1, 0});
  dict["Domain"] = reals({0.25, 0.75});
  std::vector<Object> extend{Object(Boolean{true}), Object(Boolean{false})};
  dict["Extend"] = Object(Array(std::move(extend)));
  dict["Function"] = linear_function({0}, {1});

  const auto shading = parse_shading(Object(dict), context(), color_context());
  ASSERT_NE(shading, nullptr);
  EXPECT_DOUBLE_EQ(shading->domain[0], 0.25);
  EXPECT_DOUBLE_EQ(shading->domain[1], 0.75);
  EXPECT_TRUE(shading->extend[0]);
  EXPECT_FALSE(shading->extend[1]);
}

// /Background is converted through the colour space.
TEST(PdfShading, background) {
  Dictionary dict;
  dict["ShadingType"] = Object(Integer{2});
  dict["ColorSpace"] = Object(Name{"DeviceRGB"});
  dict["Coords"] = reals({0, 0, 1, 0});
  dict["Background"] = reals({1, 0, 0});
  dict["Function"] = linear_function({0}, {1});

  const auto shading = parse_shading(Object(dict), context(), color_context());
  ASSERT_NE(shading, nullptr);
  ASSERT_TRUE(shading->has_background);
  EXPECT_NEAR(shading->background[0], 1.0, 1e-6);
  EXPECT_NEAR(shading->background[1], 0.0, 1e-6);
}

// Shading types other than 2/3 are not modelled and yield null.
TEST(PdfShading, unsupported_type_is_null) {
  Dictionary dict;
  dict["ShadingType"] = Object(Integer{1});
  dict["ColorSpace"] = Object(Name{"DeviceRGB"});
  dict["Function"] = linear_function({0}, {1});
  EXPECT_EQ(parse_shading(Object(dict), context(), color_context()), nullptr);
}

// Too few /Coords for the type makes the shading unusable.
TEST(PdfShading, short_coords_is_null) {
  Dictionary dict;
  dict["ShadingType"] = Object(Integer{2});
  dict["ColorSpace"] = Object(Name{"DeviceRGB"});
  dict["Coords"] = reals({0, 0}); // axial needs four
  dict["Function"] = linear_function({0}, {1});
  EXPECT_EQ(parse_shading(Object(dict), context(), color_context()), nullptr);
}

// An unsupported tint function makes the whole shading null (no silent wrong
// colours).
TEST(PdfShading, bad_function_is_null) {
  Dictionary fn;
  fn["FunctionType"] = Object(Integer{9}); // unsupported

  Dictionary dict;
  dict["ShadingType"] = Object(Integer{2});
  dict["ColorSpace"] = Object(Name{"DeviceRGB"});
  dict["Coords"] = reals({0, 0, 1, 0});
  dict["Function"] = Object(fn);
  EXPECT_EQ(parse_shading(Object(dict), context(), color_context()), nullptr);
}
