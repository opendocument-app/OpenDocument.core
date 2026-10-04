#include <odr/internal/number_format/number_format.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>

using namespace odr::internal::number_format;

namespace {

std::string shown(const std::string &code, const double value) {
  return Format(code).format(value);
}

} // namespace

TEST(NumberFormat, general_shows_fifteen_digits_at_most) {
  EXPECT_EQ(Format().format(0.1 + 0.2), "0.3");
  EXPECT_EQ(Format().format(1234.5), "1234.5");
  EXPECT_EQ(Format().format(-7), "-7");
  EXPECT_EQ(Format().format(0), "0");
  EXPECT_EQ(Format().format(1e20), "1E+20");
  EXPECT_EQ(Format().format(1.5e-12), "1.5E-12");
  EXPECT_EQ(shown("General", 0.00001), "0.00001");
}

TEST(NumberFormat, placeholders_pad_drop_and_blank) {
  EXPECT_EQ(shown("0.00", 3.14159), "3.14");
  EXPECT_EQ(shown("0.00", 2.675), "2.68");
  EXPECT_EQ(shown("000", 7), "007");
  EXPECT_EQ(shown("#.##", 0.5), ".5");
  EXPECT_EQ(shown("0.##", 2), "2.");
  EXPECT_EQ(shown("0.??", 2.5), "2.5 ");
  EXPECT_EQ(shown("0", 2.5), "3");
  EXPECT_EQ(shown("0", -2.5), "-3");
  EXPECT_EQ(shown("0.00", -0.001), "0.00");
  EXPECT_EQ(shown("#-0", 5), "-5");
}

TEST(NumberFormat, a_comma_groups_or_scales) {
  EXPECT_EQ(shown("#,##0", 1234567), "1,234,567");
  EXPECT_EQ(shown("#,##0.00", 1234.5), "1,234.50");
  EXPECT_EQ(shown("#,##0", 12), "12");
  EXPECT_EQ(shown("0,", 12345), "12");
  EXPECT_EQ(shown("#,##0,,", 1234567890), "1,235");
  EXPECT_EQ(shown("0.0,,\"M\"", 2500000), "2.5M");
}

TEST(NumberFormat, a_percent_scales_by_a_hundred) {
  EXPECT_EQ(shown("0%", 0.25), "25%");
  EXPECT_EQ(shown("0.00%", 0.12345), "12.35%");
}

TEST(NumberFormat, literals_stay_where_the_code_puts_them) {
  EXPECT_EQ(shown("\"$\"#,##0.00", 1234.5), "$1,234.50");
  EXPECT_EQ(shown("$#,##0", -5), "-$5");
  EXPECT_EQ(shown("000-00-0000", 123456789), "123-45-6789");
  EXPECT_EQ(shown("0\" kg\"", 3), "3 kg");
  EXPECT_EQ(shown("\\(0\\)", 3), "(3)");
  EXPECT_EQ(shown("_(0_)", 3), " 3 ");
  EXPECT_EQ(shown("[$€-407] #,##0.00", 5), "€ 5.00");
  EXPECT_EQ(shown("[Red]0", 5), "5");
}

TEST(NumberFormat, sections_split_by_sign) {
  const std::string code = "#,##0.00;(#,##0.00);\"zero\"";
  EXPECT_EQ(shown(code, 1234.5), "1,234.50");
  EXPECT_EQ(shown(code, -1234.5), "(1,234.50)");
  EXPECT_EQ(shown(code, 0), "zero");
  EXPECT_EQ(shown("0;-0;;@", 0), "");
  EXPECT_EQ(shown(";;;", 5), "");
}

TEST(NumberFormat, a_condition_picks_its_section) {
  const std::string code = "[>=1000]#,##0,\"K\";0";
  EXPECT_EQ(shown(code, 2500), "3K");
  EXPECT_EQ(shown(code, 999), "999");
  EXPECT_EQ(shown("[<0]\"neg\";\"pos\"", -1), "neg");
}

TEST(NumberFormat, scientific_notation) {
  EXPECT_EQ(shown("0.00E+00", 12345), "1.23E+04");
  EXPECT_EQ(shown("0.00E+00", 0.000123), "1.23E-04");
  EXPECT_EQ(shown("0.00E-00", 12345), "1.23E04");
  EXPECT_EQ(shown("##0.0E+0", 12345), "12.3E+3");
  EXPECT_EQ(shown("0.0E+0", 9.99), "1.0E+1");
}

TEST(NumberFormat, fractions) {
  EXPECT_EQ(shown("# ?/?", 1.5), "1 1/2");
  EXPECT_EQ(shown("# ?\?/??", 3.14159), "3 14/99");
  EXPECT_EQ(shown("?/8", 0.375), "3/8");
  EXPECT_EQ(shown("# ?/?", 2), "2    ");
}

TEST(NumberFormat, a_text_section_takes_text) {
  EXPECT_EQ(Format("0;-0;0;\"<\"@\">\"").format(std::string_view("hi")),
            "<hi>");
  EXPECT_EQ(Format("@").format(std::string_view("hi")), "hi");
  EXPECT_EQ(Format("0.00").format(std::string_view("hi")), "hi");
  EXPECT_EQ(Format("@").format(5.0), "5");
}

TEST(NumberFormat, a_malformed_code_is_refused) {
  EXPECT_THROW(Format("\"open"), std::invalid_argument);
  EXPECT_THROW(Format("[>"), std::invalid_argument);
  EXPECT_THROW(Format("0;0;0;0;0"), std::invalid_argument);
}
