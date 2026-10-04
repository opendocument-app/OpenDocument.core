#include <odr/quantity.hpp>

#include <gtest/gtest.h>

#include <clocale>
#include <cstdint>
#include <locale>

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

namespace {

class GroupedNumbers final : public std::numpunct<char> {
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\3"; }
};

class LocaleGuard final {
public:
  LocaleGuard() : m_cpp(), m_c(std::setlocale(LC_NUMERIC, nullptr)) {}
  ~LocaleGuard() {
    std::locale::global(m_cpp);
    std::setlocale(LC_NUMERIC, m_c.c_str());
  }

private:
  std::locale m_cpp;
  std::string m_c;
};

} // namespace

TEST(Quantity, ignores_numeric_locale) {
  const LocaleGuard guard;
  std::locale::global(std::locale(std::locale::classic(), new GroupedNumbers));
  if (std::setlocale(LC_NUMERIC, "de_DE.UTF-8") == nullptr) {
    std::setlocale(LC_NUMERIC, "de_DE.utf8");
  }
  EXPECT_EQ(Measure("1234.5cm"), Measure(1234.5, DynamicUnit("cm")));
  EXPECT_EQ(Quantity<std::int32_t>(1234, DynamicUnit("cm")).to_string(),
            "1234cm");
}

TEST(Quantity, checks_integral_magnitudes) {
  EXPECT_EQ(Quantity<std::int32_t>("-42cm").magnitude(), -42);
  EXPECT_THROW(Quantity<std::int32_t>("2147483648cm"), std::out_of_range);
  EXPECT_THROW(Quantity<std::uint32_t>("-1cm"), std::out_of_range);
  EXPECT_THROW(Quantity<std::uint64_t>("18446744073709551616cm"),
               std::out_of_range);
}
