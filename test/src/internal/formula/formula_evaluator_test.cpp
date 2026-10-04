#include <odr/internal/formula/formula_evaluator.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_value.hpp>

#include <internal/formula/formula_test_util.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::formula;
using namespace odr::test::formula;

TEST(FormulaEvaluator, a_text_reads_as_a_number_in_arithmetic) {
  EXPECT_EQ(ods(R"(="1"+1)"), number(2));
  EXPECT_EQ(ods("=[.A1]+1"), number(2));
  EXPECT_EQ(ods(R"(=" 1 "+1)"), number(2));
  EXPECT_EQ(ods(R"(="1e2"+1)"), number(101));
  EXPECT_EQ(ods(R"(="50%"+1)"), number(1.5));
  EXPECT_EQ(ods("=[.A2]+1"), error(ErrorType::value));
  EXPECT_EQ(ods(R"(=""+1)"), error(ErrorType::value));
  EXPECT_EQ(ods("=[.A6]+1"), error(ErrorType::value));
}

TEST(FormulaEvaluator,
     a_text_whose_number_depends_on_the_locale_has_no_answer) {
  // an application reads these by its locale: a currency, a date, an
  // accounting negative, a decimal sign or a boolean
  for (const std::string formula :
       {R"x(="$1"+1)x", R"x(="1/2/2020"+0)x", R"x(="(1)"+1)x", R"x(="1,5"+1)x",
        R"x(=".5"+1)x", R"x(="1.5"+1)x", R"x(="TRUE"+1)x", R"x(="12:30"+0)x"}) {
    EXPECT_EQ(ods(formula), std::nullopt) << formula;
  }
}

TEST(FormulaEvaluator, an_empty_cell_is_0_and_an_empty_text) {
  EXPECT_EQ(ods("=[.A3]+1"), number(1));
  EXPECT_EQ(ods(R"(=[.A3]&"x")"), text("x"));
  EXPECT_EQ(ods("=[.A3]"), number(0));
  EXPECT_EQ(ods("=[.A3]&[.A3]"), text(""));
}

TEST(FormulaEvaluator, an_error_is_the_first_one_from_the_left) {
  EXPECT_EQ(ods("=1/0"), error(ErrorType::division));
  EXPECT_EQ(ods("=1/0+NA()"), error(ErrorType::division));
  EXPECT_EQ(ods("=NA()+1/0"), error(ErrorType::not_available));
  EXPECT_EQ(ods("=1E308*10"), error(ErrorType::number));
  EXPECT_EQ(ods("=[#REF!]+1"), error(ErrorType::reference));
}

TEST(FormulaEvaluator, operators_bind_as_in_a_sheet) {
  EXPECT_EQ(ods("=-2^2"), number(4));
  EXPECT_EQ(ods("=-(2^2)"), number(-4));
  EXPECT_EQ(ods("=3/2*2"), number(3));
  EXPECT_EQ(ods("=10%"), number(0.1));
  EXPECT_EQ(ods("=2*-1"), number(-2));
}

TEST(FormulaEvaluator, a_sum_that_cancels_is_0) {
  EXPECT_EQ(ods("=0.1+0.2-0.3"), number(0));
  EXPECT_EQ(ods("=1-0.9-0.1"), number(0));
  EXPECT_EQ(ods("=0.1+0.2=0.3"), boolean(true));
  EXPECT_EQ(ods("=1+1E-16=1"), boolean(true));
  EXPECT_EQ(ods("=1E-16=0"), boolean(false));
  EXPECT_EQ(ods("=(1-0.9-0.1)*1"), number(0));
}

TEST(FormulaEvaluator, an_xlsx_sum_that_cancels_is_exact) {
  EXPECT_EQ(xlsx("=(1-0.9-0.1)*1"), number(1 - 0.9 - 0.1));
  EXPECT_EQ(xlsx("=1-(0.9+0.1)"), number(0));
  EXPECT_EQ(xlsx("=0.1+0.2=0.3"), boolean(true));
  // Excel gives 0 here, but -2.78E-17 for =(1-0.9-0.1)
  EXPECT_EQ(xlsx("=1-0.9-0.1"), std::nullopt);
}

TEST(FormulaEvaluator, a_power_follows_the_dialect) {
  EXPECT_EQ(ods("=0^0"), number(1));
  EXPECT_EQ(xlsx("=0^0"), error(ErrorType::number));
  EXPECT_EQ(ods("=0^-1"), error(ErrorType::number));
  EXPECT_EQ(xlsx("=0^-1"), error(ErrorType::division));
  // LibreOffice takes the odd root: -2
  EXPECT_EQ(ods("=(-8)^(1/3)"), std::nullopt);
  EXPECT_EQ(xlsx("=(-8)^(1/3)"), error(ErrorType::number));
  EXPECT_EQ(ods("=(-8)^2"), number(64));
}

TEST(FormulaEvaluator, a_number_reads_as_15_significant_digits_in_a_text) {
  EXPECT_EQ(ods(R"(=1/3&"")"), text("0.333333333333333"));
  EXPECT_EQ(ods(R"(=12345.678901234567&"")"), text("12345.6789012346"));
  EXPECT_EQ(ods(R"(=0.1+0.2&"")"), text("0.3"));
  EXPECT_EQ(ods(R"(=-0&"")"), text("0"));
  EXPECT_EQ(ods(R"(=1&2)"), text("12"));
  // LibreOffice writes 1E+020 and 1E-020, Excel 1E+20 and 1E-20
  EXPECT_EQ(ods(R"(=1E20&"")"), std::nullopt);
  EXPECT_EQ(ods(R"(=1E-20&"")"), std::nullopt);
}

TEST(FormulaEvaluator, a_boolean_is_a_number_in_libreoffice) {
  EXPECT_EQ(ods(R"(=TRUE()&"")"), text("1"));
  EXPECT_EQ(xlsx(R"(=TRUE&"")"), text("TRUE"));
  EXPECT_EQ(ods("=TRUE()=1"), boolean(true));
  EXPECT_EQ(xlsx("=TRUE=1"), boolean(false));
  EXPECT_EQ(ods(R"(="a"<TRUE())"), boolean(false));
  EXPECT_EQ(xlsx(R"(="a"<TRUE)"), boolean(true));
  EXPECT_EQ(ods("=TRUE()+TRUE()"), number(2));
  EXPECT_EQ(ods("=[.A5]=1"), boolean(true));
}

TEST(FormulaEvaluator, a_number_sorts_before_a_text) {
  EXPECT_EQ(ods(R"(=1<"a")"), boolean(true));
  EXPECT_EQ(ods("=[.A1]=1"), boolean(false));
  EXPECT_EQ(ods(R"(=""=0)"), boolean(false));
}

TEST(FormulaEvaluator, an_empty_cell_compares_as_what_it_meets) {
  EXPECT_EQ(ods(R"(=[.A3]="")"), boolean(true));
  EXPECT_EQ(ods("=[.A3]=0"), boolean(true));
  EXPECT_EQ(ods("=[.A3]=FALSE()"), boolean(true));
  EXPECT_EQ(ods("=[.A3]<FALSE()"), boolean(false));
  EXPECT_EQ(ods(R"(=[.A3]<"a")"), boolean(true));
  EXPECT_EQ(ods("=[.A3]=[.A3]"), boolean(true));
}

TEST(FormulaEvaluator, a_text_compares_by_the_case_the_settings_state) {
  EXPECT_EQ(ods(R"(="a"="A")"), boolean(false));
  EXPECT_EQ(xlsx(R"(="a"="A")"), boolean(true));
  EXPECT_EQ(ods(R"(="a"<"B")"), boolean(true));
  EXPECT_EQ(ods(R"(="B">"a")"), boolean(true));
  EXPECT_EQ(ods(R"(="a"<"A")"), boolean(true));
  EXPECT_EQ(ods(R"(="A"<"a")"), boolean(false));
  EXPECT_EQ(ods(R"(="ab"<"Aa")"), boolean(false));
  EXPECT_EQ(ods(R"(="a1"<"a10")"), boolean(true));
  EXPECT_EQ(ods(R"(="1"<"a")"), boolean(true));
  EXPECT_EQ(xlsx(R"(="a"<"A")"), boolean(false));
}

TEST(FormulaEvaluator, a_text_the_collator_would_order_has_no_answer) {
  EXPECT_EQ(ods(R"(="a b"<"ab")"), std::nullopt);
  EXPECT_EQ(xlsx(R"(="é"="É")"), boolean(true));
  EXPECT_EQ(xlsx(R"(="ж"="Ж")"), std::nullopt);
  EXPECT_EQ(ods(R"(="é"="é")"), boolean(true));
}

TEST(FormulaEvaluator, a_range_reads_the_cell_the_formula_crosses) {
  EXPECT_EQ(ods("=[.A1:.A40]"), number(7));
  EXPECT_EQ(ods("=[.A4:.A5]"), error(ErrorType::value));
  EXPECT_EQ(ods("=[.A4]:[.A5]"), error(ErrorType::value));
  EXPECT_EQ(xlsx("=A1:A40*2"), number(14));
}

TEST(FormulaEvaluator, the_reference_operators_make_a_range) {
  EXPECT_EQ(ods("=[.A4:.A5]![.A4]"), number(2));
  EXPECT_EQ(ods("=[.A4:.A5]![.B1]"), error(ErrorType::null));
  EXPECT_EQ(ods("=[.A4]~[.A5]"), error(ErrorType::value));
}

TEST(FormulaEvaluator, a_reference_names_another_sheet) {
  EXPECT_EQ(ods("=[$T.A1]*2"), number(10));
  EXPECT_EQ(xlsx("=t!A1*2"), number(10));
  EXPECT_EQ(xlsx("=nope!A1"), std::nullopt);
  EXPECT_EQ(xlsx("=[1]t!A1"), std::nullopt);
}

TEST(FormulaEvaluator, an_array_is_read_by_its_first_element) {
  EXPECT_EQ(ods("={1;2}"), number(1));
  EXPECT_EQ(ods("={1;2}+1"), number(2));
  EXPECT_EQ(xlsx("={1,2;3,4}*2"), number(2));
}

TEST(FormulaEvaluator, what_it_does_not_know_has_no_answer) {
  EXPECT_EQ(ods("=NOSUCHFUNCTION(1)"), std::nullopt);
  EXPECT_EQ(xlsx("=total*2"), std::nullopt);

  Cells source = cells();
  source.stale.insert(SheetPosition(0, 0, 3));
  EXPECT_EQ(ods("=[.A4]+1", source), std::nullopt);
  EXPECT_EQ(ods("=[.A5]+1", source), number(2));
}

TEST(FormulaEvaluator, a_function_name_drops_the_prefix_of_its_format) {
  EXPECT_EQ(xlsx("=_xlfn.TRUE()"), boolean(true));
  EXPECT_EQ(ods("=org.openoffice.na()"), error(ErrorType::not_available));
}

TEST(FormulaEvaluator, a_name_stands_for_its_expression) {
  Cells source = cells();
  source.names["total"] = "[$s.$A$4:.$A$5]";
  source.names["rate"] = "of:=0.5*2";
  source.names["twice"] = "$$Total";
  source.names["relative"] = "[$s.A4]";
  source.names["loop"] = "$$Loop";

  EXPECT_EQ(ods("=SUM($$Total)*$$Rate", source), number(3));
  EXPECT_EQ(xlsx("=SUM(Total)*rate", source), number(2));
  EXPECT_EQ(ods("=SUM($$Twice)", source), number(3));
  // a relative reference reads from a base cell the formats state apart
  EXPECT_EQ(ods("=$$Relative", source), std::nullopt);
  EXPECT_EQ(ods("=$$Loop", source), std::nullopt);
  EXPECT_EQ(ods("=$$Unknown", source), std::nullopt);
}

TEST(FormulaEvaluator, a_whole_column_reaches_past_the_extent_of_its_sheet) {
  // `t` states one row, and the formula sits in row 30
  EXPECT_EQ(xlsx("=t!A:A"), number(0));
  EXPECT_EQ(xlsx("=SUM(t!A:A)"), number(5));
  EXPECT_EQ(xlsx("=SUM(t!1:1)"), number(5));
  EXPECT_EQ(xlsx("=SUM(4:5)"), number(2));
}

TEST(FormulaEvaluator, a_libreoffice_boolean_in_a_join_follows_its_version) {
  const auto joined = [](const std::optional<bool> boolean_word) {
    return evaluated(R"(=TRUE()&"")", Syntax::opendocument,
                     Settings{.dialect = Dialect::libreoffice,
                              .boolean_word = boolean_word});
  };
  EXPECT_EQ(joined(false), text("1"));
  EXPECT_EQ(joined(true), text("TRUE"));
  EXPECT_EQ(joined(std::nullopt), std::nullopt);
  // CONCATENATE spells it as a number in every version
  EXPECT_EQ(evaluated(R"(=CONCATENATE(TRUE();""))", Syntax::opendocument,
                      Settings{.dialect = Dialect::libreoffice}),
            text("1"));
}

TEST(FormulaEvaluator, an_excel_comparison_near_the_15th_digit_has_no_answer) {
  // approxEqual decides apart, and the 15 digits a cell shows are the same
  EXPECT_EQ(xlsx("=1+4E-15=1"), std::nullopt);
  EXPECT_EQ(ods("=1+4E-15=1"), boolean(false));
  EXPECT_EQ(xlsx("=0.1+0.2=0.3"), boolean(true));
}
