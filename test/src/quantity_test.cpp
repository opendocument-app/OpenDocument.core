#include <odr/quantity.hpp>

#include <locale_util.hpp>

#include <gtest/gtest.h>

#include <clocale>
#include <cstdint>
#include <locale>
#include <numbers>

using namespace odr;

TEST(Quantity, construct) {
  EXPECT_EQ(Measure("10ms"), Measure(10, DynamicUnit("ms")));
  EXPECT_EQ(Measure("10 ms"), Measure(10, DynamicUnit("ms")));
  EXPECT_EQ(Measure("1.5em"), Measure(1.5, DynamicUnit("em")));
  EXPECT_EQ(Measure(" -2.5e-3 ex"), Measure(-0.0025, DynamicUnit("ex")));
  EXPECT_THROW(Measure("cm"), std::invalid_argument);
  EXPECT_THROW(Measure("1e999cm"), std::invalid_argument);
}

/// A default-constructed unit used to leave the unit pointer null, so rendering
/// such a quantity dereferenced null instead of emitting a bare number.
TEST(DynamicUnit, default_constructed_is_the_unitless_unit) {
  EXPECT_EQ(DynamicUnit(), DynamicUnit(""));
  EXPECT_EQ(DynamicUnit().name(), "");
  EXPECT_EQ(DynamicUnit().to_string(), "");
}

TEST(Quantity, default_unit_renders_a_bare_number) {
  EXPECT_EQ(Measure(0, DynamicUnit()).to_string(), "0");
  EXPECT_EQ(Measure(1.5, {}).to_string(), "1.5");
}

/// A fallback measure has to compare equal to the same value parsed from text,
/// which only holds if both end up on the registered empty unit.
TEST(Quantity, default_unit_equals_parsed_unitless) {
  EXPECT_EQ(Measure(0, DynamicUnit()), Measure("0"));
  EXPECT_EQ(Measure(2.5, {}), Measure("2.5"));
}

TEST(Quantity, ignores_numeric_locale) {
  const odr::test::LocaleGuard guard;
  std::locale::global(
      std::locale(std::locale::classic(), new odr::test::GroupedNumbers));
  if (std::setlocale(LC_NUMERIC, "de_DE.UTF-8") == nullptr) {
    std::setlocale(LC_NUMERIC, "de_DE.utf8");
  }
  EXPECT_EQ(Measure("1234.5cm"), Measure(1234.5, DynamicUnit("cm")));
  EXPECT_EQ(Quantity<std::int32_t>(1234, DynamicUnit("cm")).to_string(),
            "1234cm");
}

TEST(DynamicUnit, knows_dimensions) {
  EXPECT_EQ(DynamicUnit("cm").known(), KnownUnit::centimetre);
  EXPECT_EQ(DynamicUnit("cm"), DynamicUnit(KnownUnit::centimetre));
  EXPECT_EQ(DynamicUnit(KnownUnit::pixel).name(), "px");
  EXPECT_EQ(DynamicUnit("pt").dimension(), Dimension::length);
  EXPECT_EQ(DynamicUnit("grad").dimension(), Dimension::angle);
  EXPECT_EQ(DynamicUnit("%").dimension(), Dimension::dimensionless);
  EXPECT_EQ(DynamicUnit().known(), KnownUnit::none);
  EXPECT_EQ(DynamicUnit("em").known(), KnownUnit::unknown);
  EXPECT_EQ(DynamicUnit("em").dimension(), Dimension::unknown);
  EXPECT_THROW((void)DynamicUnit(KnownUnit::unknown), std::invalid_argument);
}

TEST(Quantity, converts_within_a_dimension) {
  EXPECT_EQ(Measure("1in").magnitude_in(KnownUnit::point), 72);
  EXPECT_EQ(Measure("1in").magnitude_in(KnownUnit::centimetre), 2.54);
  EXPECT_EQ(Measure("72pt").magnitude_in(KnownUnit::pixel), 96);
  EXPECT_EQ(Measure("2.5cm").magnitude_in(KnownUnit::centimetre), 2.5);
  EXPECT_DOUBLE_EQ(Measure("180deg").magnitude_in(KnownUnit::radian),
                   std::numbers::pi);
  EXPECT_DOUBLE_EQ(Measure("100grad").magnitude_in(KnownUnit::degree), 90);
  EXPECT_EQ(Measure("50%").magnitude_in(KnownUnit::none), 0.5);
  EXPECT_EQ(Quantity<std::int32_t>("3in").magnitude_in(KnownUnit::point), 216);
  EXPECT_EQ(Measure("1in").to(KnownUnit::point), Measure("72pt"));
}

TEST(Quantity, refuses_a_conversion_across_dimensions) {
  EXPECT_THROW((void)Measure("1cm").magnitude_in(KnownUnit::degree),
               std::invalid_argument);
  EXPECT_THROW((void)Measure("1em").magnitude_in(KnownUnit::point),
               std::invalid_argument);
  EXPECT_THROW((void)Measure("1").to(KnownUnit::point), std::invalid_argument);
}

TEST(Quantity, checks_integral_magnitudes) {
  EXPECT_EQ(Quantity<std::int32_t>("-42cm").magnitude(), -42);
  EXPECT_THROW(Quantity<std::int32_t>("2147483648cm"), std::out_of_range);
  EXPECT_THROW(Quantity<std::uint32_t>("-1cm"), std::out_of_range);
  EXPECT_THROW(Quantity<std::uint64_t>("18446744073709551616cm"),
               std::out_of_range);
}
