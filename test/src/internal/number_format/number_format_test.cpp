#include <odr/internal/number_format/number_format.hpp>

#include <gtest/gtest.h>

#include <limits>
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
  EXPECT_EQ(Format("0.00").format(std::numeric_limits<double>::infinity()),
            "inf");
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
  EXPECT_EQ(shown("##0.0E+0", 999.99), "1.0E+3");
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

TEST(NumberFormat, a_date_counts_its_days_from_1900) {
  EXPECT_EQ(shown("yyyy-mm-dd", 45658), "2025-01-01");
  EXPECT_EQ(shown("m/d/yy", 45658.75), "1/1/25");
  EXPECT_EQ(shown("d mmm yyyy", 61), "1 Mar 1900");
  EXPECT_EQ(shown("yyyy-mm-dd", 60), "1900-02-29");
  EXPECT_EQ(shown("yyyy-mm-dd", 59), "1900-02-28");
  EXPECT_EQ(shown("yyyy-mm-dd", 1), "1900-01-01");
  EXPECT_EQ(shown("dddd, mmmm d", 45658), "Wednesday, January 1");
  EXPECT_EQ(shown("ddd mmmmm", 45658), "Wed J");
  EXPECT_EQ(Format("yyyy-mm-dd").format(0, Epoch::from_1904), "1904-01-01");
  EXPECT_EQ(Format("dddd").format(0, Epoch::from_1904), "Friday");
}

TEST(NumberFormat, a_time_is_the_fraction_of_a_day) {
  EXPECT_EQ(shown("hh:mm:ss", 0.75), "18:00:00");
  EXPECT_EQ(shown("h:mm AM/PM", 0.75), "6:00 PM");
  EXPECT_EQ(shown("h:mm am/pm", 0.25), "6:00 am");
  EXPECT_EQ(shown("h A/P", 0), "12 A");
  EXPECT_EQ(shown("mm:ss.00", 1.5 / 86400), "00:01.50");
  EXPECT_EQ(shown("[h]:mm", 1.5), "36:00");
  EXPECT_EQ(shown("[mm]:ss", 0.5 / 24), "30:00");
  EXPECT_EQ(shown("hh:mm", 0.99999999), "00:00");
  EXPECT_EQ(shown("yyyy-mm-dd hh:mm", 45658.99999999), "2025-01-02 00:00");
}

TEST(NumberFormat, m_is_a_minute_next_to_an_hour_or_a_second) {
  EXPECT_EQ(shown("h:m", 0.5 + 5.0 / 1440), "12:5");
  EXPECT_EQ(shown("m:ss", 65.0 / 86400), "1:05");
  EXPECT_EQ(shown("m/d", 45658), "1/1");
}

TEST(NumberFormat, a_format_says_whether_it_shows_a_date) {
  EXPECT_EQ(Format("yyyy-mm-dd").category(), Category::date);
  EXPECT_EQ(Format("[$-409]h:mm AM/PM;@").category(), Category::time);
  EXPECT_EQ(Format("0.00").category(), Category::number);
  EXPECT_EQ(Format().category(), Category::number);
  EXPECT_EQ(shown("yyyy-mm-dd", -1), "-1");
  EXPECT_EQ(shown("yyyy-mm-dd", 2958465), "9999-12-31");
  EXPECT_EQ(shown("yyyy-mm-dd", 1e12), "1000000000000");
}

TEST(NumberFormat, a_locale_writes_its_own_point_and_grouping) {
  const Symbols german = symbols_of("de-DE");
  EXPECT_EQ(Format("#,##0.00").format(1234.5, Epoch::from_1900, german),
            "1.234,50");
  EXPECT_EQ(Format().format(1.5, Epoch::from_1900, german), "1,5");
  EXPECT_EQ(Format("0.00E+00").format(12345, Epoch::from_1900, german),
            "1,23E+04");
  EXPECT_EQ(Format("#,##0").format(1234, Epoch::from_1900, symbols_of("fr-FR")),
            "1 234");
  EXPECT_EQ(
      Format("#,##0.0").format(1234.5, Epoch::from_1900, symbols_of("de-CH")),
      "1’234.5");
  EXPECT_EQ(symbols_of("en-US").decimal, ".");
  EXPECT_EQ(symbols_of("ja").group, ",");
  EXPECT_EQ(symbols_of("es-MX").decimal, ".");
  EXPECT_EQ(symbols_of("pt-BR").group, ".");
}
