#include <odr/internal/util/number_util.hpp>

#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

#include <gtest/gtest.h>

using namespace odr::internal::util::number;

TEST(ParseInteger, checks_the_destination_range_without_rounding) {
  EXPECT_EQ(parse_integer<std::int64_t>("9223372036854775807"),
            std::numeric_limits<std::int64_t>::max());
  EXPECT_EQ(parse_integer<std::int64_t>("-9223372036854775808"),
            std::numeric_limits<std::int64_t>::min());
  EXPECT_FALSE(parse_integer<std::int64_t>("9223372036854775808"));
  EXPECT_FALSE(parse_integer<std::int64_t>("-9223372036854775809"));
  EXPECT_EQ(parse_integer<std::uint64_t>("18446744073709551615"),
            std::numeric_limits<std::uint64_t>::max());
  EXPECT_FALSE(parse_integer<std::uint64_t>("18446744073709551616"));
  EXPECT_EQ(parse_integer<std::int8_t>("127"), 127);
  EXPECT_EQ(parse_integer<std::int8_t>("-128"), -128);
  EXPECT_FALSE(parse_integer<std::int8_t>("128"));
  EXPECT_FALSE(parse_integer<std::int8_t>("-129"));
  EXPECT_EQ(parse_integer<std::uint8_t>("255"), 255);
  EXPECT_FALSE(parse_integer<std::uint8_t>("256"));
}

TEST(ParseInteger, accepts_plus_only_when_requested) {
  EXPECT_EQ(parse_integer<std::int32_t>("-42"), -42);
  EXPECT_EQ(parse_integer<std::int32_t>("-42", {.allow_plus = true}), -42);
  EXPECT_EQ(parse_integer<std::int32_t>("+42", {.allow_plus = true}), 42);
  EXPECT_EQ(parse_integer<std::uint32_t>("+42", {.allow_plus = true}), 42U);
  EXPECT_FALSE(parse_integer<std::int32_t>("+42"));
  EXPECT_FALSE(parse_integer<std::uint32_t>("+42"));
  for (const auto token : std::array{"+", "-", "++1", "+-1", "-+1", "--1"}) {
    SCOPED_TRACE(token);
    EXPECT_FALSE(parse_integer<std::int32_t>(token, {.allow_plus = true}));
    EXPECT_FALSE(parse_integer<std::uint32_t>(token, {.allow_plus = true}));
  }
  for (const auto token : std::array{"-1", "-0"}) {
    SCOPED_TRACE(token);
    EXPECT_FALSE(parse_integer<std::uint32_t>(token));
    EXPECT_FALSE(parse_integer<std::uint32_t>(token, {.allow_plus = true}));
  }
}

TEST(ParseInteger, requires_a_whole_token_without_whitespace) {
  EXPECT_EQ(parse_integer<std::int32_t>("00042"), 42);
  EXPECT_EQ(parse_integer<std::uint32_t>("0"), 0U);
  EXPECT_FALSE(parse_integer<std::int32_t>(std::string_view{}));
  EXPECT_FALSE(parse_integer<std::int32_t>(std::string_view("1\0x", 3)));
  for (const auto token :
       std::array{"", " 1", "1 ", "1\n", "1x", "1.0", "1e2", "0x10", "+ 1"}) {
    SCOPED_TRACE(token);
    EXPECT_FALSE(parse_integer<std::int32_t>(token, {.allow_plus = true}));
  }
}

TEST(ParseInteger, reads_digits_in_the_requested_base) {
  EXPECT_EQ(parse_integer<std::uint32_t>("fF", {.base = 16}), 255U);
  EXPECT_EQ(parse_integer<std::int32_t>("-ff", {.base = 16}), -255);
  EXPECT_EQ(
      parse_integer<std::uint32_t>("+ff", {.base = 16, .allow_plus = true}),
      255U);
  EXPECT_EQ(parse_integer<std::uint32_t>("101", {.base = 2}), 5U);
  EXPECT_EQ(parse_integer<std::uint32_t>("z", {.base = 36}), 35U);
  EXPECT_FALSE(parse_integer<std::uint32_t>("0xff", {.base = 16}));
  EXPECT_FALSE(parse_integer<std::uint32_t>("100000000", {.base = 16}));
  EXPECT_FALSE(parse_integer<std::uint32_t>("2", {.base = 2}));
  for (const std::int32_t base : std::array<std::int32_t, 4>{-1, 0, 1, 37}) {
    EXPECT_FALSE(parse_integer<std::uint32_t>("1", {.base = base}));
  }
}

TEST(Parse, reads_a_decimal_in_the_classic_spelling) {
  EXPECT_EQ(parse("1234.5"), std::optional(1234.5));
  EXPECT_EQ(parse("-0.25"), std::optional(-0.25));
  EXPECT_EQ(parse("2.5e-3"), std::optional(2.5e-3));
}

TEST(Parse, allows_blanks_around_the_number) {
  EXPECT_EQ(parse(" \t1234.5\n"), std::optional(1234.5));
}

/// Anything it cannot read whole is refused, so a number spelled in another
/// locale is not truncated to the part before the separator.
TEST(Parse, refuses_what_it_cannot_read_whole) {
  EXPECT_FALSE(parse("1,5").has_value());
  EXPECT_FALSE(parse("12pt").has_value());
  EXPECT_FALSE(parse("").has_value());
}

TEST(ToStringSignificant, trims_trailing_zeros) {
  EXPECT_EQ(to_string_significant(1.5, 7), "1.5");
  EXPECT_EQ(to_string_significant(2.0, 7), "2");
  EXPECT_EQ(to_string_significant(0.0, 7), "0");
}

TEST(ToStringSignificant, keeps_requested_significant_digits) {
  EXPECT_EQ(to_string_significant(1.0429, 7), "1.0429");
  EXPECT_EQ(to_string_significant(-6734.61, 7), "-6734.61");
  EXPECT_EQ(to_string_significant(0.007, 7), "0.007");
}

TEST(ToStringSignificant, rounds_beyond_the_requested_digits) {
  EXPECT_EQ(to_string_significant(1.23456789, 4), "1.235");
  EXPECT_EQ(to_string_significant(191.31, 4), "191.3");
}

/// The reason the digit count is bounded: a value that arrived as a `float`
/// carries about 7 digits, and asking for more exposes the binary
/// representation (an xlsx column width of `68.55` becomes `68.550003`).
TEST(ToStringSignificant, hides_float_representation_noise) {
  const auto from_float = static_cast<double>(68.55F);
  EXPECT_EQ(to_string_significant(from_float, 7), "68.55");
  EXPECT_EQ(to_string_significant(from_float, 10), "68.55000305");
}

/// CSS and SVG lengths reject scientific notation, so it must never appear
/// however large or small the value is.
TEST(ToStringSignificant, never_uses_scientific_notation) {
  EXPECT_EQ(to_string_significant(13421095.26, 7), "13421095");
  EXPECT_EQ(to_string_significant(1e21, 7), "1000000000000000000000");
  EXPECT_EQ(to_string_significant(0.00001, 7), "0.00001");
}

TEST(ToStringSignificant, passes_through_non_finite) {
  EXPECT_EQ(to_string_significant(std::nan(""), 7), "nan");
  EXPECT_EQ(to_string_significant(HUGE_VAL, 7), "inf");
}

TEST(ToStringSignificant, bounds_extreme_precision) {
  EXPECT_EQ(
      to_string_significant(0.5, std::numeric_limits<std::int32_t>::max()),
      "0.5");
  EXPECT_EQ(
      to_string_significant(1234, std::numeric_limits<std::int32_t>::min()),
      "1234");
}

TEST(ToInteger, takes_only_whole_finite_values) {
  EXPECT_EQ(to_integer<std::int32_t>(-12.0), std::optional<std::int32_t>(-12));
  EXPECT_EQ(to_integer<std::int32_t>(1.5), std::nullopt);
  EXPECT_EQ(to_integer<std::int32_t>(std::nan("")), std::nullopt);
  EXPECT_EQ(to_integer<std::int32_t>(std::numeric_limits<double>::infinity()),
            std::nullopt);
}

TEST(ToInteger, takes_exactly_the_range_of_the_type) {
  EXPECT_EQ(to_integer<std::int32_t>(-2147483648.0),
            std::numeric_limits<std::int32_t>::min());
  EXPECT_EQ(to_integer<std::int32_t>(2147483647.0),
            std::numeric_limits<std::int32_t>::max());
  EXPECT_EQ(to_integer<std::int32_t>(-2147483649.0), std::nullopt);
  EXPECT_EQ(to_integer<std::int32_t>(2147483648.0), std::nullopt);

  EXPECT_EQ(to_integer<std::uint32_t>(4294967295.0),
            std::numeric_limits<std::uint32_t>::max());
  EXPECT_EQ(to_integer<std::uint32_t>(4294967296.0), std::nullopt);
  EXPECT_EQ(to_integer<std::uint32_t>(-1.0), std::nullopt);

  // 2^63 is where `max()` rounds to as a double, so a `max()` bound takes it.
  EXPECT_EQ(to_integer<std::int64_t>(std::ldexp(1.0, 63)), std::nullopt);
  EXPECT_EQ(to_integer<std::int64_t>(-std::ldexp(1.0, 63)),
            std::numeric_limits<std::int64_t>::min());
}
