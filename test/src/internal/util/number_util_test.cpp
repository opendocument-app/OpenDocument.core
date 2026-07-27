#include <odr/internal/util/number_util.hpp>

#include <cmath>

#include <gtest/gtest.h>

using namespace odr::internal::util::number;

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
