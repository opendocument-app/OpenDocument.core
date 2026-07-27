#include <odr/quantity.hpp>

#include <gtest/gtest.h>

using namespace odr;

TEST(Quantity, construct) {
  Quantity<double>{"10ms"};
  Quantity<double>{"10 ms"};
}

/// A default-constructed unit used to leave the unit pointer null, so anything
/// that rendered such a quantity — the geometry fallback for a shape missing
/// its `svg:*` attribute, for one — dereferenced null instead of emitting a
/// bare number.
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
