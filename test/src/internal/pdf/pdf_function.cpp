#include <odr/internal/pdf/pdf_function.hpp>

#include <odr/internal/pdf/pdf_object.hpp>
#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <array>
#include <cstdint>
#include <limits>
#include <sstream>
#include <string>
#include <utility>
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

Object integers(std::initializer_list<std::int64_t> values) {
  std::vector<Object> holder;
  for (const std::int64_t value : values) {
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
  ASSERT_NE(fn, nullptr);
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
  ASSERT_NE(fn, nullptr);
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
  dict["Bounds"] = reals({1});
  const auto endpoint = parse_function(Object(dict), context());
  ASSERT_NE(endpoint, nullptr);
  EXPECT_DOUBLE_EQ(endpoint->eval({1})[0], 1);
  // an empty first subdomain is degenerate, not malformed
  dict["Bounds"] = reals({0});
  EXPECT_NE(parse_function(Object(dict), context()), nullptr);
}

// Type 0: a 1-D, 8-bit sample table, linearly interpolated and decoded.
TEST(PdfFunction, sampled_linear) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{0});
  dict["Domain"] = reals({0, 1});
  dict["Range"] = reals({0, 1});
  dict["Size"] = integers({2});
  for (const auto &[bits, samples] :
       std::array{std::pair{8, std::string("\x00\xff", 2)},
                  std::pair{12, std::string("\x00\x0f\xff", 3)}}) {
    SCOPED_TRACE(bits);
    dict["BitsPerSample"] = Object(Integer{bits});
    const auto fn = parse_function(Object(dict), context(samples));
    ASSERT_NE(fn, nullptr);
    EXPECT_DOUBLE_EQ(fn->eval({0.0})[0], 0.0);
    EXPECT_DOUBLE_EQ(fn->eval({1.0})[0], 1.0);
    EXPECT_NEAR(fn->eval({0.5})[0], 0.5, 1e-6);
  }
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

TEST(PdfFunction, malformed_layout_is_null) {
  for (const std::string entries :
       {"/FunctionType 3 /Domain [0] /Functions []",
        "/FunctionType 2 /Domain [0 1 2] /N 1",
        "/FunctionType 2 /Domain [1 0] /N 1",
        "/FunctionType 2 /Domain [-1 1] /N 0.5",
        "/FunctionType 2 /Domain [0 1] /N -1",
        "/FunctionType 2 /Domain [0 1] /Range [0] /N 1",
        "/FunctionType 4294967298 /Domain [0 1] /N 1",
        "/FunctionType 3 /Domain [0 1] /Functions ["
        "<< /FunctionType 2 /Domain [0 1] /N 1 >>] /Bounds [0.5] /Encode [0 1]",
        "/FunctionType 0 /Domain [0 1] /Range [0 1] /Size [-1] /BitsPerSample "
        "8",
        "/FunctionType 0 /Domain [0 1] /Range [0 1] /Size [2] /BitsPerSample 3",
        "/FunctionType 0 /Domain [0 1] /Range [0 1] /Size [2] "
        "/BitsPerSample 4294967304",
        "/FunctionType 0 /Domain [0 1] /Range [0 1] "
        "/Size [9223372036854775807] /BitsPerSample 32",
        "/FunctionType 0 /Domain [0 1 0 1] /Range [0 1] "
        "/Size [4294967296 4294967296] /BitsPerSample 8",
        "/FunctionType 0 /Domain [0 1] /Range [0 1] /Size [3] /BitsPerSample "
        "8"}) {
    SCOPED_TRACE(entries);
    std::istringstream stream("<< " + entries + " >>");
    ObjectParser parser(stream);
    EXPECT_EQ(
        parse_function(parser.read_object(), context(std::string(2, '\0'))),
        nullptr);
  }
}

TEST(PdfFunction, recursive_stitching_is_null) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{3});
  dict["Domain"] = reals({0, 1});
  dict["Functions"] = Object(Array({Object(ObjectReference{1, 0})}));
  dict["Bounds"] = reals({});
  dict["Encode"] = reals({0, 1});
  auto ctx = context();
  std::size_t resolutions = 0;
  ctx.resolve = [&](const Object &) {
    if (++resolutions > 100) {
      throw std::runtime_error("unbounded function recursion");
    }
    return Object(dict);
  };
  EXPECT_EQ(parse_function(Object(ObjectReference{1, 0}), ctx), nullptr);
}

// `/C0` and `/C1` of unequal length keep the outputs both state.
TEST(PdfFunction, exponential_uses_the_shorter_of_c0_and_c1) {
  std::istringstream stream(
      "<< /FunctionType 2 /Domain [0 1] /C0 [0 1] /C1 [1] /N 1 >>");
  ObjectParser parser(stream);
  const auto fn = parse_function(parser.read_object(), context());
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->eval({0.5}).size(), 1u);
}

TEST(PdfFunction, calculator_tokens_and_braces) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{4});
  dict["Domain"] = reals({0, 1});
  dict["Range"] = reals({0, 1});
  const auto fn = parse_function(
      Object(dict),
      context("% leading { comment\n{ +.75% pop } comment\r exch sub }% tail"));
  ASSERT_NE(fn, nullptr);
  EXPECT_DOUBLE_EQ(fn->eval({0.25})[0], 0.5);
  for (const std::string program : {"{ 1..2 }", "{ + }", "{ 1e999 }", "{ 1",
                                    "1 }", "{ 1 } }", "1", "{1}{2}"}) {
    SCOPED_TRACE(program);
    EXPECT_EQ(parse_function(Object(dict), context(program)), nullptr);
  }
  EXPECT_EQ(parse_function(Object(dict), context(std::string(1000, '{') + "1" +
                                                 std::string(1000, '}'))),
            nullptr);
}

TEST(PdfFunction, calculator_stack_operations) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{4});
  dict["Domain"] = reals({0, 1});
  dict["Range"] = reals({-10, 10});
  for (const auto &[program, expected] :
       std::array{std::pair{"{ pop 2 3 2 copy add add add }", 10.0},
                  std::pair{"{ pop 2 3 1 index add add }", 7.0},
                  std::pair{"{ pop 1 2 3 3 -1 roll pop sub }", -1.0},
                  std::pair{"{ pop 1 2 3 3 2147483647 roll pop sub }", 2.0},
                  std::pair{"{ 1 }", 1.0}}) {
    SCOPED_TRACE(program);
    const auto fn = parse_function(Object(dict), context(program));
    ASSERT_NE(fn, nullptr);
    EXPECT_DOUBLE_EQ(fn->eval({0.5})[0], expected);
  }
}

TEST(PdfFunction, calculator_invalid_execution_returns_zero) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{4});
  dict["Domain"] = reals({0, 1});
  dict["Range"] = reals({0, 1});
  for (const std::string program :
       {"{ pop {} }", "{ pop }", "{ pop 1e308 dup mul }", "{ -1 copy }",
        "{ 1e100 copy }", "{ 0.5 copy }", "{ 2 copy }", "{ -1 index }",
        "{ 1e100 index }", "{ 1 index }", "{ -1 0 roll }", "{ 1 1e100 roll }",
        "{ 1 0.5 roll }", "{ pop { dup true exch if } dup true exch if }"}) {
    SCOPED_TRACE(program);
    const auto fn = parse_function(Object(dict), context(program));
    ASSERT_NE(fn, nullptr);
    EXPECT_EQ(fn->eval({0.5}), std::vector<double>{0});
  }
  std::string growth = "{ ";
  for (std::size_t i = 0; i < 13; ++i) {
    growth += std::to_string(std::size_t{1} << i) + " copy ";
  }
  const auto fn = parse_function(Object(dict), context(growth + "}"));
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->eval({0.5}), std::vector<double>{0});
}

TEST(PdfFunction, calculator_instruction_limit) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{4});
  dict["Domain"] = reals({0, 1});
  dict["Range"] = reals({0, 1});
  std::string program = "{ ";
  for (std::size_t i = 0; i < 100001; ++i) {
    program += "cvr ";
  }
  const auto fn = parse_function(Object(dict), context(program + "}"));
  ASSERT_NE(fn, nullptr);
  EXPECT_EQ(fn->eval({0.5}), std::vector<double>{0});
}

TEST(PdfFunction, interpolation_avoids_intermediate_overflow) {
  const double limit = std::numeric_limits<double>::max();
  Dictionary sampled;
  sampled["FunctionType"] = Object(Integer{0});
  sampled["Domain"] = reals({-limit, limit});
  sampled["Range"] = reals({-limit, limit});
  sampled["Size"] = integers({2});
  sampled["BitsPerSample"] = Object(Integer{8});
  const auto grid =
      parse_function(Object(sampled), context(std::string("\0\xff", 2)));
  ASSERT_NE(grid, nullptr);
  EXPECT_DOUBLE_EQ(grid->eval({0})[0], 0);
  EXPECT_DOUBLE_EQ(grid->eval({-limit})[0], -limit);
  EXPECT_DOUBLE_EQ(grid->eval({limit})[0], limit);
  EXPECT_THROW(grid->eval({std::numeric_limits<double>::quiet_NaN()}),
               std::invalid_argument);

  Dictionary exponential;
  exponential["FunctionType"] = Object(Integer{2});
  exponential["Domain"] = reals({0, 1});
  exponential["C0"] = reals({-limit});
  exponential["C1"] = reals({limit});
  exponential["N"] = Object(Real{1});
  const auto linear = parse_function(Object(exponential), context());
  ASSERT_NE(linear, nullptr);
  EXPECT_DOUBLE_EQ(linear->eval({0.5})[0], 0);
}

TEST(PdfFunction, rejects_non_finite_values) {
  Dictionary dict;
  dict["FunctionType"] = Object(Integer{2});
  dict["Domain"] = reals({0, 2});
  dict["N"] = Object(Real{2048});
  const auto fn = parse_function(Object(dict), context());
  ASSERT_NE(fn, nullptr);
  for (const double value : {std::numeric_limits<double>::quiet_NaN(),
                             std::numeric_limits<double>::infinity()}) {
    EXPECT_THROW(fn->eval({value}), std::invalid_argument);
    for (const char *key : {"C0", "C1"}) {
      Dictionary invalid = dict;
      invalid[key] = reals({value});
      EXPECT_EQ(parse_function(Object(invalid), context()), nullptr);
    }
  }
  EXPECT_THROW(fn->eval({2}), std::runtime_error);
  dict["C0"] = reals({1});
  const auto constant = parse_function(Object(dict), context());
  ASSERT_NE(constant, nullptr);
  EXPECT_DOUBLE_EQ(constant->eval({2})[0], 1);
  dict["Range"] = reals({0, 1});
  for (const double start : {0.0, 1.0}) {
    dict["C0"] = reals({start});
    dict["C1"] = reals({1 - start});
    const auto clipped = parse_function(Object(dict), context());
    ASSERT_NE(clipped, nullptr);
    EXPECT_DOUBLE_EQ(clipped->eval({2})[0], 1 - start);
  }
}
