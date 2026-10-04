#include <internal/formula/formula_test_util.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace odr::internal::formula;
using namespace odr::test::formula;

// The results of `ods` are the ones LibreOffice computes for the formula,
// the results of `xlsx` the ones Excel documents. A serial counts days from
// 1899-12-30: 45658 is 2025-01-01.

TEST(FormulaFunctionsDate, a_date_runs_over_into_the_next_month) {
  EXPECT_EQ(ods("=DATE(2020;1;1)"), number(43831));
  EXPECT_EQ(ods("=DATE(2020;13;1)"), number(44197));
  EXPECT_EQ(ods("=DATE(2020;1;0)"), number(43830));
  EXPECT_EQ(ods("=DATE(2020.9;1.9;1.9)"), number(43831));
  EXPECT_EQ(ods("=DATE(2020;2;29)"), number(43890));
  EXPECT_EQ(ods("=DATE(1899;12;31)"), number(1));
  EXPECT_EQ(ods("=DATE(10000;1;1)"), number(2958466));
  EXPECT_EQ(xlsx("=DATE(10000,1,1)"), error(ErrorType::number));
}

TEST(FormulaFunctionsDate, a_short_year_follows_the_dialect) {
  // LibreOffice puts a two-digit year at or past the null year, 1930
  EXPECT_EQ(ods("=DATE(20;1;1)"), number(43831));
  EXPECT_EQ(ods("=DATE(29;1;1)"), number(47119));
  EXPECT_EQ(ods("=DATE(30;1;1)"), number(10959));
  EXPECT_EQ(ods("=DATE(99;1;1)"), number(36161));
  // Excel adds 1900 to a year before 1900
  EXPECT_EQ(xlsx("=DATE(20,1,1)"), number(7306));
  // LibreOffice: #VALUE! and Err:502
  EXPECT_EQ(ods("=DATE(1000;1;1)"), std::nullopt);
  EXPECT_EQ(ods("=DATE(-1;1;1)"), std::nullopt);
}

TEST(FormulaFunctionsDate, the_parts_of_a_date) {
  EXPECT_EQ(ods("=YEAR(45658.9)"), number(2025));
  EXPECT_EQ(ods("=YEAR(-1)"), number(1899));
  EXPECT_EQ(ods("=YEAR(0)"), number(1899));
  EXPECT_EQ(ods("=MONTH(60)"), number(2));
  EXPECT_EQ(ods("=DAY(60)"), number(28));
  EXPECT_EQ(ods("=DAY(61)"), number(1));
  // Excel counts a 1900-02-29 that never was before serial 61
  EXPECT_EQ(xlsx("=DAY(60)"), std::nullopt);
  EXPECT_EQ(xlsx("=YEAR(-1)"), error(ErrorType::number));
}

TEST(FormulaFunctionsDate, the_parts_of_a_time) {
  EXPECT_EQ(ods("=HOUR(0.5)"), number(12));
  EXPECT_EQ(ods("=HOUR(0.99999)"), number(23));
  EXPECT_EQ(ods("=HOUR(-0.25)"), number(18));
  EXPECT_EQ(ods("=HOUR(1.75)"), number(18));
  EXPECT_EQ(ods("=MINUTE(0.5000001)"), number(0));
  // LibreOffice: 0, 1, 2 and 59. It seems to round the second and to
  // truncate the minute, which no documentation states.
  EXPECT_EQ(ods("=SECOND(0.4999999999)"), std::nullopt);
  EXPECT_EQ(ods("=SECOND(0.00001)"), std::nullopt);
  EXPECT_EQ(ods("=SECOND(1/86400*1.5)"), std::nullopt);
  EXPECT_EQ(ods("=MINUTE(0.041666666)"), std::nullopt);
}

TEST(FormulaFunctionsDate, a_time_from_its_parts) {
  EXPECT_EQ(ods("=TIME(25;0;0)"), number(1.0 / 24));
  EXPECT_EQ(ods("=TIME(1.9;0;0)"), number(1.9 * 3600 / 86400));
  EXPECT_EQ(xlsx("=TIME(1.9,0,0)"), number(1.0 / 24));
  // LibreOffice: Err:502
  EXPECT_EQ(ods("=TIME(-1;0;0)"), std::nullopt);
  EXPECT_EQ(xlsx("=TIME(0,0,-1)"), error(ErrorType::number));
}

TEST(FormulaFunctionsDate, the_day_of_the_week) {
  EXPECT_EQ(ods("=WEEKDAY(1)"), number(1));
  EXPECT_EQ(ods("=WEEKDAY(45658)"), number(4));
  EXPECT_EQ(ods("=WEEKDAY(45658;2)"), number(3));
  EXPECT_EQ(ods("=WEEKDAY(45658;3)"), number(2));
  EXPECT_EQ(ods("=WEEKDAY(45658;11)"), number(3));
  EXPECT_EQ(ods("=WEEKDAY(-1)"), number(6));
  EXPECT_EQ(ods("=WEEKDAY(45658;4)"), std::nullopt);
  EXPECT_EQ(xlsx("=WEEKDAY(45658,4)"), error(ErrorType::number));
}

TEST(FormulaFunctionsDate, a_date_some_months_on) {
  EXPECT_EQ(ods("=EDATE(45688;1)"), number(45716));
  EXPECT_EQ(ods("=EDATE(45658.7;1)"), number(45689));
  EXPECT_EQ(ods("=EOMONTH(45658;0)"), number(45688));
  EXPECT_EQ(ods("=EOMONTH(45658;-1)"), number(45657));
  EXPECT_EQ(ods("=EOMONTH(45658;1.9)"), number(45716));
}

TEST(FormulaFunctionsDate, the_days_between_two_dates) {
  EXPECT_EQ(ods("=DAYS(45658;45600)"), number(58));
  const std::optional<Value> fraction = ods("=DAYS(45658.9;45600.1)");
  ASSERT_TRUE(fraction.has_value() && fraction->holds<double>());
  EXPECT_NEAR(fraction->get<double>(), 58.8, 1e-9);
  EXPECT_EQ(xlsx("=DAYS(45658.9,45600.1)"), number(58));
  EXPECT_EQ(ods(R"(=DATEDIF(45000;45658;"Y"))"), number(1));
  EXPECT_EQ(ods(R"(=DATEDIF(45000;45658;"M"))"), number(21));
  EXPECT_EQ(ods(R"(=DATEDIF(45000;45658;"D"))"), number(658));
  EXPECT_EQ(ods(R"(=DATEDIF(45658;45000;"D"))"), std::nullopt);
  EXPECT_EQ(xlsx(R"(=DATEDIF(45658,45000,"D"))"), error(ErrorType::number));
  EXPECT_EQ(ods(R"(=DATEDIF(45000;45658;"MD"))"), std::nullopt);
}

TEST(FormulaFunctionsDate, a_date_as_a_text_has_no_answer) {
  // LibreOffice: 2020, as it reads an ISO 8601 date
  EXPECT_EQ(ods(R"(=YEAR("2020-01-02"))"), std::nullopt);
}
