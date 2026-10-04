#include <internal/formula/formula_test_util.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <numbers>
#include <optional>
#include <string>

using namespace odr::internal::formula;
using namespace odr::test::formula;

// The results of `ods` are the ones LibreOffice computes for the formula,
// over column A of `cells()`: `1` and `abc` as texts, nothing, 2, true, and
// an empty text. The results of `xlsx` are the ones Excel documents.

TEST(FormulaFunctionsMath, a_sum_reads_the_numbers_of_a_range) {
  // LibreOffice reads the boolean as 1, Excel skips it
  EXPECT_EQ(ods("=SUM([.A1:.A6])"), number(3));
  EXPECT_EQ(xlsx("=SUM(A1:A6)"), number(2));
  EXPECT_EQ(ods("=SUM([.A2])"), number(0));
  EXPECT_EQ(ods("=SUM()"), number(0));
  EXPECT_EQ(ods("=SUM(TRUE();1)"), number(2));
  EXPECT_EQ(ods("=SUM([.A4];NA())"), error(ErrorType::not_available));
  EXPECT_EQ(ods("=SUM(1/0;NA())"), error(ErrorType::division));
}

TEST(FormulaFunctionsMath, a_text_argument_follows_the_dialect) {
  // LibreOffice: #VALUE!, and Err:504 for MAX
  EXPECT_EQ(ods(R"(=SUM("2";1))"), std::nullopt);
  EXPECT_EQ(ods(R"(=MAX("3";1))"), std::nullopt);
  EXPECT_EQ(xlsx(R"(=SUM("2",1))"), number(3));
  EXPECT_EQ(xlsx(R"(=MAX("3",1))"), number(3));
  EXPECT_EQ(xlsx(R"(=SUM("abc"))"), error(ErrorType::value));
}

TEST(FormulaFunctionsMath, a_sum_that_cancels_is_0_in_libreoffice) {
  EXPECT_EQ(ods("=SUM(0.1;0.2;-0.3)"), number(0));
  EXPECT_EQ(ods("=SUM(0.1;0.2;0.3;0.4;-1)"), number(0));
  EXPECT_EQ(ods("=SUM(1E20;1;-1E20)"), number(0));
  EXPECT_EQ(xlsx("=SUM(0.1,0.2,-0.3)"), std::nullopt);
}

TEST(FormulaFunctionsMath, a_libreoffice_sum_keeps_what_a_large_number_hides) {
  // LibreOffice's `KahanSum`; adding left to right gives 0.5
  EXPECT_EQ(ods("=SUM(1;1E20;-1E20;0.5)"), number(1.5));
}

TEST(FormulaFunctionsMath, an_average_needs_a_number) {
  EXPECT_EQ(ods("=AVERAGE([.A1:.A6])"), number(1.5));
  EXPECT_EQ(ods("=AVERAGE([.A3])"), error(ErrorType::division));
  EXPECT_EQ(ods("=AVERAGE(TRUE();3)"), number(2));
  EXPECT_EQ(ods("=AVERAGE([.A1:.A6];NA())"), error(ErrorType::not_available));
}

TEST(FormulaFunctionsMath, the_extremes_of_nothing_are_0) {
  EXPECT_EQ(ods("=MIN([.A1:.A3])"), number(0));
  EXPECT_EQ(ods("=MAX([.A2])"), number(0));
  EXPECT_EQ(ods("=MAX([.A5];0)"), number(1));
  EXPECT_EQ(ods("=PRODUCT([.A1:.A6])"), number(2));
  EXPECT_EQ(ods("=PRODUCT([.A2])"), number(0));
  EXPECT_EQ(ods("=SUMSQ(3;4)"), number(25));
  EXPECT_EQ(ods("=SUMSQ([.A1:.A6])"), number(5));
}

TEST(FormulaFunctionsMath, a_count_counts_what_its_rule_says) {
  EXPECT_EQ(ods("=COUNT([.A1:.A6])"), number(2));
  EXPECT_EQ(xlsx("=COUNT(A1:A6)"), number(1));
  EXPECT_EQ(ods(R"(=COUNT("1";1;TRUE()))"), number(3));
  EXPECT_EQ(ods("=COUNT()"), number(0));
  EXPECT_EQ(ods("=COUNTA([.A1:.A6])"), number(5));
  EXPECT_EQ(ods(R"(=COUNTA("";1))"), number(2));
  EXPECT_EQ(ods(R"(=COUNTA([.A3];""))"), number(1));
}

TEST(FormulaFunctionsMath, an_empty_text_is_blank_in_excel) {
  EXPECT_EQ(xlsx("=COUNTBLANK(A1:A6)"), number(2));
  // LibreOffice counts one, but would count an empty text a formula made
  EXPECT_EQ(ods("=COUNTBLANK([.A1:.A6])"), std::nullopt);
  EXPECT_EQ(ods("=COUNTBLANK([.A1:.A5])"), number(1));
  EXPECT_EQ(xlsx("=COUNTBLANK(B1:B10)"), number(10));
}

TEST(FormulaFunctionsMath, the_order_statistics) {
  EXPECT_EQ(ods("=MEDIAN(1;2;3;4)"), number(2.5));
  EXPECT_EQ(ods("=MEDIAN([.A3])"), error(ErrorType::value));
  EXPECT_EQ(xlsx("=MEDIAN(A3)"), error(ErrorType::number));
  EXPECT_EQ(ods("=LARGE([.A4:.A5];1)"), number(2));
  EXPECT_EQ(ods("=SMALL({3;1;2};2)"), number(2));
  EXPECT_EQ(ods("=LARGE({3;1;2};1.5)"), number(2));
  EXPECT_EQ(xlsx("=LARGE({3,1,2},1.5)"), std::nullopt);
  EXPECT_EQ(ods("=SMALL([.A2];1)"), error(ErrorType::value));
  EXPECT_EQ(xlsx("=SMALL({3,1,2},4)"), error(ErrorType::number));
}

TEST(FormulaFunctionsMath, the_spread_of_numbers) {
  EXPECT_EQ(ods("=STDEV(1;2;3)"), number(1));
  EXPECT_EQ(ods("=VAR(1)"), error(ErrorType::division));
  EXPECT_EQ(ods("=STDEVP(2;4)"), number(1));
  EXPECT_EQ(ods("=VARP(1)"), number(0));
}

TEST(FormulaFunctionsMath, rounding_reads_the_number_as_a_sheet_shows_it) {
  EXPECT_EQ(ods("=ROUND(2.675;2)"), number(2.68));
  EXPECT_EQ(ods("=ROUND(1.005;2)"), number(1.01));
  EXPECT_EQ(ods("=ROUND(-2.5;0)"), number(-3));
  EXPECT_EQ(ods("=ROUND(0.5;0)"), number(1));
  EXPECT_EQ(ods("=ROUND(-0.5;0)"), number(-1));
  EXPECT_EQ(ods("=ROUND(2.5)"), number(3));
  EXPECT_EQ(ods("=ROUND(2.5;0.9)"), number(3));
  EXPECT_EQ(ods("=ROUND(1234.5;-2)"), number(1200));
  EXPECT_EQ(ods("=ROUND(1E20;2)"), number(1e20));
  EXPECT_EQ(ods("=ROUNDUP(0.1*3;1)"), number(0.3));
  EXPECT_EQ(ods("=ROUNDUP(-1.21;1)"), number(-1.3));
  EXPECT_EQ(ods("=ROUNDDOWN(-1.25;1)"), number(-1.2));
  EXPECT_EQ(ods("=TRUNC(-1.57;1)"), number(-1.5));
  EXPECT_EQ(ods("=TRUNC(2.5)"), number(2));
  EXPECT_EQ(ods("=INT(-1.5)"), number(-2));
  EXPECT_EQ(ods("=INT(0.1*3*10)"), number(3));
  EXPECT_EQ(ods("=INT(-0.0000000000000001)"), number(-1));
}

TEST(FormulaFunctionsMath, a_number_at_the_edge_of_a_rounding_has_no_answer) {
  // LibreOffice: 1, as it reads the number to fewer digits than 15
  EXPECT_EQ(ods("=ROUNDUP(1.0000000000001;0)"), std::nullopt);
  // Excel documents no reading to 15 digits outside a rounding
  EXPECT_EQ(ods("=INT(0.3/0.1)"), number(3));
  EXPECT_EQ(xlsx("=INT(0.3/0.1)"), std::nullopt);
  EXPECT_EQ(xlsx("=INT(0.1*3*10)"), number(3));
  EXPECT_EQ(xlsx("=MOD(3,0.1)"), std::nullopt);
  EXPECT_EQ(xlsx("=MOD(-3,2)"), number(1));
}

TEST(FormulaFunctionsMath, a_remainder_takes_the_sign_of_the_divisor) {
  EXPECT_EQ(ods("=MOD(-3;2)"), number(1));
  EXPECT_EQ(ods("=MOD(3;-2)"), number(-1));
  EXPECT_EQ(ods("=MOD(-0.5;1)"), number(0.5));
  EXPECT_EQ(ods("=MOD(3;0.1)"), number(0));
  EXPECT_EQ(ods("=MOD(1;0)"), error(ErrorType::division));
  const std::optional<Value> fraction = ods("=MOD(5.1;1)");
  ASSERT_TRUE(fraction.has_value() && fraction->holds<double>());
  EXPECT_DOUBLE_EQ(fraction->get<double>(), 0.0999999999999996);
  EXPECT_EQ(ods("=QUOTIENT(-7;2)"), number(-3));
  // LibreOffice: Err:502
  EXPECT_EQ(ods("=QUOTIENT(1;0)"), std::nullopt);
  EXPECT_EQ(xlsx("=QUOTIENT(1,0)"), error(ErrorType::division));
}

TEST(FormulaFunctionsMath, an_argument_outside_the_domain_follows_the_dialect) {
  // LibreOffice: Err:502
  EXPECT_EQ(ods("=SQRT(-1)"), std::nullopt);
  EXPECT_EQ(ods("=LN(0)"), std::nullopt);
  EXPECT_EQ(ods("=FACT(-1)"), std::nullopt);
  EXPECT_EQ(ods("=LOG(10;1)"), std::nullopt);
  EXPECT_EQ(xlsx("=SQRT(-1)"), error(ErrorType::number));
  EXPECT_EQ(xlsx("=LN(0)"), error(ErrorType::number));
  EXPECT_EQ(xlsx("=LOG(10,1)"), error(ErrorType::division));
  EXPECT_EQ(ods("=ASIN(2)"), error(ErrorType::number));
  EXPECT_EQ(ods("=EXP(1000)"), error(ErrorType::number));
  EXPECT_EQ(ods("=FACT(171)"), error(ErrorType::value));
  EXPECT_EQ(xlsx("=FACT(171)"), error(ErrorType::number));
  EXPECT_EQ(ods("=ATAN2(0;0)"), number(0));
  EXPECT_EQ(xlsx("=ATAN2(0,0)"), error(ErrorType::division));
}

TEST(FormulaFunctionsMath, the_functions_of_one_number) {
  EXPECT_EQ(ods(R"(=ABS("-2"))"), number(2));
  EXPECT_EQ(ods("=ABS(TRUE())"), number(1));
  EXPECT_EQ(ods("=SIGN(-0)"), number(0));
  EXPECT_EQ(ods(R"(=SIGN("a"))"), error(ErrorType::value));
  EXPECT_EQ(ods("=LOG(8;2)"), number(3));
  EXPECT_EQ(ods("=LOG(100)"), number(2));
  EXPECT_EQ(ods("=FACT(5.9)"), number(120));
  EXPECT_EQ(ods("=FACT(0)"), number(1));
  EXPECT_EQ(ods("=EVEN(1.5)"), number(2));
  EXPECT_EQ(ods("=EVEN(-1.5)"), number(-2));
  EXPECT_EQ(ods("=EVEN(0)"), number(0));
  EXPECT_EQ(ods("=ODD(-2)"), number(-3));
  EXPECT_EQ(ods("=ODD(0)"), number(1));
  EXPECT_EQ(ods("=POWER(0;0)"), number(1));
  EXPECT_EQ(ods("=DEGREES(PI())"), number(180));
  EXPECT_EQ(ods("=RADIANS(180)"), number(std::numbers::pi));
  EXPECT_EQ(ods("=TAN(PI()/4)"), number(std::tan(std::numbers::pi / 4)));
  EXPECT_EQ(ods("=ATAN2(1;1)"), number(std::numbers::pi / 4));
  EXPECT_EQ(ods("=ATAN2(-1;0)"), number(std::numbers::pi));
}

TEST(FormulaFunctionsMath, a_sum_product_reads_its_arguments_as_arrays) {
  EXPECT_EQ(ods("=SUMPRODUCT([.A4:.A5];[.A4:.A5])"), number(5));
  EXPECT_EQ(ods("=SUMPRODUCT(([.A1:.A6]>0)*1)"), number(5));
  EXPECT_EQ(ods("=SUMPRODUCT([.A1:.A6])"), number(3));
  EXPECT_EQ(ods("=SUMPRODUCT([.A4:.A5]>1)"), number(1));
  // Excel sorts a boolean after every number
  EXPECT_EQ(xlsx("=SUMPRODUCT((A4:A5>1)*1)"), number(2));
  EXPECT_EQ(ods("=SUMPRODUCT([.A1:.A2];[.A4:.A5])"), number(0));
  EXPECT_EQ(ods(R"(=SUMPRODUCT({1;2};{"a";3}))"), number(6));
  EXPECT_EQ(ods("=SUMPRODUCT({1;2};{3;4;5})"), error(ErrorType::value));
  // LibreOffice maps ABS over the array: 3
  EXPECT_EQ(ods("=SUMPRODUCT(ABS({-1;2}))"), std::nullopt);
}

TEST(FormulaFunctionsMath, a_range_outside_an_array_context_reads_one_cell) {
  EXPECT_EQ(ods("=SUM([.A1:.A6]*2)"), error(ErrorType::value));
}
