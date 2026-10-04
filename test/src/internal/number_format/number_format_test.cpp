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
  EXPECT_EQ(shown("General%", 0.25), "25%");
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

/// The names are the ones LibreOffice shows for the same date style.
TEST(NumberFormat, a_locale_writes_its_own_month_and_day_names) {
  const auto shown_in = [](const std::string_view code,
                           const std::string_view locale) {
    // 2025-03-15, a Saturday
    return Format(code).format(45731, Epoch::from_1900, symbols_of(locale));
  };
  EXPECT_EQ(shown_in("dddd, d. mmmm yyyy", "de-DE"), "Samstag, 15. März 2025");
  EXPECT_EQ(shown_in("ddd d. mmm", "de-DE"), "Sa 15. Mär");
  // a month after its day is declined
  EXPECT_EQ(shown_in("mmmm yyyy", "ru-RU"), "Март 2025");
  EXPECT_EQ(shown_in("d mmmm yyyy", "ru-RU"), "15 марта 2025");
  EXPECT_EQ(shown_in("mmmmm", "ru-RU"), "М");
  EXPECT_EQ(shown_in("d mmmm", "fi-FI"), "15 maaliskuuta");
  EXPECT_EQ(shown_in("mmmm", "no"), "mars");
  EXPECT_EQ(shown_in("dddd, mmmm d", "ja"), "Saturday, March 15");
}

/// What LibreOffice shows for the same code in an xlsx.
TEST(NumberFormat, a_locale_id_names_the_months_and_days) {
  const auto shown_in = [](const std::string_view code,
                           const Symbols &symbols = {}) {
    // 2025-03-15, a Saturday
    return Format(code).format(45731, Epoch::from_1900, symbols);
  };
  EXPECT_EQ(shown_in("[$-419]d mmmm yyyy"), "15 марта 2025");
  EXPECT_EQ(shown_in("[$-419]mmmm"), "Март");
  EXPECT_EQ(shown_in("[$-407]dddd, d. mmmm"), "Samstag, 15. März");
  EXPECT_EQ(shown_in("[$-40C]ddd d mmm"), "sam. 15 mars");
  EXPECT_EQ(shown_in("[$-415]d mmmm"), "15 marca");
  EXPECT_EQ(shown_in("[$-0414]mmmm"), "mars");
  // the upper bits pick a calendar and digits, not a language
  EXPECT_EQ(shown_in("[$-1010409]mmmm"), "March");
  EXPECT_EQ(shown_in("[$-F800]mmmm"), "March");
  // the code's language beats the locale's, and leaves the signs alone
  EXPECT_EQ(shown_in("[$-419]mmmm", symbols_of("de-DE")), "Март");
  EXPECT_EQ(Format("[$€-407]#,##0.00").format(1234.5), "€1,234.50");
}

TEST(NumberFormat, a_serial_is_days_since_1899_12_30) {
  EXPECT_EQ(days_from_civil(1899, 12, 30), 0);
  EXPECT_EQ(days_from_civil(2025, 1, 1), 45658);
  EXPECT_EQ(days_from_civil(1900, 1, 1), 2);
  EXPECT_EQ(days_from_civil(1800, 1, 1), -36522);

  EXPECT_DOUBLE_EQ(days_from_serial(45658.5, Epoch::from_1900), 45658.5);
  EXPECT_DOUBLE_EQ(days_from_serial(1, Epoch::from_1900), 2);
  EXPECT_DOUBLE_EQ(days_from_serial(60, Epoch::from_1900), 61);
  EXPECT_DOUBLE_EQ(days_from_serial(0, Epoch::from_1904), 1462);
  EXPECT_DOUBLE_EQ(serial_from_days(2, Epoch::from_1900), 1);
  EXPECT_DOUBLE_EQ(serial_from_days(45658, Epoch::from_1904), 44196);
}
