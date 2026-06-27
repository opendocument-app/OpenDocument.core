#include <odr/internal/pdf/pdf_function.hpp>

#include <odr/internal/pdf/pdf_object.hpp>

#include <string>
#include <vector>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

namespace {

// A context with no indirection: objects resolve to themselves, and every
// stream returns the captured bytes (the function under test is the only one).
FunctionContext context(std::string stream = {}) {
  FunctionContext ctx;
  ctx.resolve = [](const Object &object) { return object; };
  ctx.load_stream = [data = std::move(stream)](const Object &) { return data; };
  return ctx;
}

Object reals(std::initializer_list<double> values) {
  std::vector<Object> holder;
  for (const double value : values) {
    holder.emplace_back(Real{value});
  }
  return Object(Array(std::move(holder)));
}

Object integers(std::initializer_list<int> values) {
  std::vector<Object> holder;
  for (const int value : values) {
    holder.emplace_back(Integer{value});
  }
  return Object(Array(std::move(holder)));
}

} // namespace

// Type 2: linear interpolation between C0 and C1 (N = 1).
TEST(PdfFunction, exponential_linear) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{2});
  dict["Domain"] = reals({0, 1});
  dict["C0"] = reals({0, 0, 0});
  dict["C1"] = reals({1, 0.5, 0});
  dict["N"] = Object(Real{1});

  const auto fn = parse_function(Object(dict), context());
  ASSERT_NE(fn, nullptr);
  const std::vector<double> out = fn->eval({0.5});
  ASSERT_EQ(out.size(), 3);
  EXPECT_DOUBLE_EQ(out[0], 0.5);
  EXPECT_DOUBLE_EQ(out[1], 0.25);
  EXPECT_DOUBLE_EQ(out[2], 0.0);
}

// Type 2 with N = 2: the input is raised to the exponent before interpolation.
TEST(PdfFunction, exponential_power) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{2});
  dict["Domain"] = reals({0, 1});
  dict["C0"] = reals({0});
  dict["C1"] = reals({1});
  dict["N"] = Object(Real{2});

  const auto fn = parse_function(Object(dict), context());
  EXPECT_DOUBLE_EQ(fn->eval({0.5})[0], 0.25);
}

// Inputs outside the domain are clipped before evaluation.
TEST(PdfFunction, clips_domain) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{2});
  dict["Domain"] = reals({0, 1});
  dict["C0"] = reals({0});
  dict["C1"] = reals({1});
  dict["N"] = Object(Real{1});

  const auto fn = parse_function(Object(dict), context());
  EXPECT_DOUBLE_EQ(fn->eval({2.0})[0], 1.0);  // clipped to 1
  EXPECT_DOUBLE_EQ(fn->eval({-1.0})[0], 0.0); // clipped to 0
}

// Type 3: stitch two subfunctions at the bound, each over its encoded
// subdomain.
TEST(PdfFunction, stitching) {
  Dictionary sub0;
  sub0["FunctionType"] = Object(Integer{2});
  sub0["Domain"] = reals({0, 1});
  sub0["C0"] = reals({0});
  sub0["C1"] = reals({1});
  sub0["N"] = Object(Real{1});
  Dictionary sub1;
  sub1["FunctionType"] = Object(Integer{2});
  sub1["Domain"] = reals({0, 1});
  sub1["C0"] = reals({1});
  sub1["C1"] = reals({0});
  sub1["N"] = Object(Real{1});

  std::vector<Object> functions{Object(sub0), Object(sub1)};
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{3});
  dict["Domain"] = reals({0, 1});
  dict["Functions"] = Object(Array(std::move(functions)));
  dict["Bounds"] = reals({0.5});
  dict["Encode"] = reals({0, 1, 0, 1});

  const auto fn = parse_function(Object(dict), context());
  ASSERT_NE(fn, nullptr);
  // 0.25 -> sub0, encoded 0.25/0.5 = 0.5 -> 0.5.
  EXPECT_DOUBLE_EQ(fn->eval({0.25})[0], 0.5);
  // 0.75 -> sub1, encoded (0.75-0.5)/0.5 = 0.5 -> 1 + 0.5*(0-1) = 0.5.
  EXPECT_DOUBLE_EQ(fn->eval({0.75})[0], 0.5);
  // endpoints of each segment
  EXPECT_DOUBLE_EQ(fn->eval({0.0})[0], 0.0);
  EXPECT_DOUBLE_EQ(fn->eval({1.0})[0], 0.0);
}

// Type 0: a 1-D, 8-bit sample table, linearly interpolated and decoded.
TEST(PdfFunction, sampled_linear) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{0});
  dict["Domain"] = reals({0, 1});
  dict["Range"] = reals({0, 1});
  dict["Size"] = integers({2});
  dict["BitsPerSample"] = Object(Integer{8});

  const std::string samples("\x00\xff", 2); // sample[0]=0, sample[1]=255
  const auto fn = parse_function(Object(dict), context(samples));
  ASSERT_NE(fn, nullptr);
  EXPECT_DOUBLE_EQ(fn->eval({0.0})[0], 0.0);
  EXPECT_DOUBLE_EQ(fn->eval({1.0})[0], 1.0);
  EXPECT_NEAR(fn->eval({0.5})[0], 0.5, 1e-6); // (0 + 255)/2 / 255
}

// Type 4: a PostScript calculator program, one input and one output.
TEST(PdfFunction, postscript_invert) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{4});
  dict["Domain"] = reals({0, 1});
  dict["Range"] = reals({0, 1});

  const auto fn = parse_function(Object(dict), context("{ 1 exch sub }"));
  ASSERT_NE(fn, nullptr);
  EXPECT_DOUBLE_EQ(fn->eval({0.25})[0], 0.75);
  EXPECT_DOUBLE_EQ(fn->eval({1.0})[0], 0.0);
}

// Type 4 with two inputs and arithmetic; outputs clipped to the range.
TEST(PdfFunction, postscript_two_inputs) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{4});
  dict["Domain"] = reals({0, 1, 0, 1});
  dict["Range"] = reals({0, 2});

  const auto fn = parse_function(Object(dict), context("{ add }"));
  ASSERT_NE(fn, nullptr);
  EXPECT_DOUBLE_EQ(fn->eval({0.3, 0.4})[0], 0.7);
}

// Type 4 control flow: ifelse selects a branch on a boolean.
TEST(PdfFunction, postscript_ifelse) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{4});
  dict["Domain"] = reals({0, 1});
  dict["Range"] = reals({0, 10});

  // x 0.5 gt { 9 } { 1 } ifelse  -> 9 when x > 0.5, else 1
  const auto fn =
      parse_function(Object(dict), context("{ 0.5 gt { 9 } { 1 } ifelse }"));
  ASSERT_NE(fn, nullptr);
  EXPECT_DOUBLE_EQ(fn->eval({0.8})[0], 9.0);
  EXPECT_DOUBLE_EQ(fn->eval({0.2})[0], 1.0);
}

// An unsupported function type yields a null function rather than throwing.
TEST(PdfFunction, unsupported_type_is_null) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{9});
  EXPECT_EQ(parse_function(Object(dict), context()), nullptr);
}
