#include <internal/formula/formula_test_util.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace odr::internal::formula;
using namespace odr::test::formula;

// The results of `ods` are the ones LibreOffice computes for the formula,
// over column A of `cells()`: `1` and `abc` as texts, nothing, 2, true, and
// an empty text. The results of `xlsx` are the ones Excel documents.

TEST(FormulaFunctionsLogic, a_condition_takes_one_branch) {
  EXPECT_EQ(ods("=IF([.A3];1;2)"), number(2));
  EXPECT_EQ(ods("=IF(1)"), boolean(true));
  EXPECT_EQ(ods("=IF(0;1)"), boolean(false));
  EXPECT_EQ(ods("=IF(0;1;)"), number(0));
  EXPECT_EQ(ods(R"(=IF("abc";1;2))"), error(ErrorType::value));
  EXPECT_EQ(ods(R"(=IF("";1;2))"), error(ErrorType::value));
  // the branch not taken is not read
  EXPECT_EQ(ods("=IF(1;2;NOSUCHFUNCTION())"), number(2));
  EXPECT_EQ(ods("=IF(1;[.A1:.A2])"), error(ErrorType::value));
}

TEST(FormulaFunctionsLogic, an_error_can_be_caught) {
  EXPECT_EQ(ods(R"(=IFERROR(1/0;"x"))"), text("x"));
  EXPECT_EQ(ods("=IFERROR(2;1/0)"), number(2));
  EXPECT_EQ(ods("=IFNA(NA();1)"), number(1));
  EXPECT_EQ(ods("=IFNA(1/0;1)"), error(ErrorType::division));
}

TEST(FormulaFunctionsLogic, the_truth_of_several_values) {
  EXPECT_EQ(ods("=AND([.A1:.A2])"), error(ErrorType::value));
  EXPECT_EQ(ods(R"(=AND(1;"a"))"), error(ErrorType::value));
  EXPECT_EQ(ods("=AND([.A3])"), error(ErrorType::value));
  EXPECT_EQ(ods("=AND([.A5])"), boolean(true));
  EXPECT_EQ(ods("=OR([.A4:.A5])"), boolean(true));
  EXPECT_EQ(ods("=OR(0;[.A1])"), boolean(false));
  EXPECT_EQ(ods("=XOR(TRUE();TRUE())"), boolean(false));
  EXPECT_EQ(ods(R"(=NOT("a"))"), error(ErrorType::value));
  EXPECT_EQ(xlsx(R"(=AND("TRUE"))"), std::nullopt);
}

TEST(FormulaFunctionsLogic, a_choice_by_its_number) {
  EXPECT_EQ(ods(R"(=CHOOSE(2.9;"a";"b";"c"))"), text("b"));
  // LibreOffice: Err:502
  EXPECT_EQ(ods("=CHOOSE(0;1)"), std::nullopt);
  EXPECT_EQ(xlsx("=CHOOSE(0,1)"), error(ErrorType::value));
}

TEST(FormulaFunctionsLogic, the_type_of_a_value) {
  EXPECT_EQ(ods("=ISBLANK([.A3])"), boolean(true));
  EXPECT_EQ(ods("=ISBLANK([.A6])"), boolean(false));
  EXPECT_EQ(ods("=ISTEXT([.A1])"), boolean(true));
  EXPECT_EQ(ods("=ISNONTEXT([.A3])"), boolean(true));
  EXPECT_EQ(ods("=ISERROR(1/0)"), boolean(true));
  EXPECT_EQ(ods("=ISERROR([.A2])"), boolean(false));
  EXPECT_EQ(ods("=ISERR(NA())"), boolean(false));
  EXPECT_EQ(ods("=ISNA(NA())"), boolean(true));
  EXPECT_EQ(ods("=ISEVEN(3)"), boolean(false));
  EXPECT_EQ(ods("=ISODD(2.5)"), boolean(false));
}

TEST(FormulaFunctionsLogic, a_boolean_cell_is_a_number_in_libreoffice) {
  EXPECT_EQ(ods("=ISNUMBER(TRUE())"), boolean(true));
  EXPECT_EQ(ods("=ISNUMBER([.A5])"), boolean(true));
  EXPECT_EQ(ods("=ISLOGICAL(TRUE())"), boolean(true));
  EXPECT_EQ(ods("=ISLOGICAL([.A5])"), boolean(false));
  EXPECT_EQ(xlsx("=ISNUMBER(TRUE)"), boolean(false));
  EXPECT_EQ(xlsx("=ISLOGICAL(A5)"), boolean(true));
}

TEST(FormulaFunctionsLogic, a_value_as_a_number_or_a_text) {
  EXPECT_EQ(ods(R"(=N("a"))"), number(0));
  EXPECT_EQ(ods("=N(TRUE())"), number(1));
  EXPECT_EQ(ods("=N([.A1])"), number(0));
  EXPECT_EQ(ods("=N(1/0)"), error(ErrorType::division));
  EXPECT_EQ(ods("=T(1)"), text(""));
  EXPECT_EQ(ods(R"(=T("a"))"), text("a"));
}

TEST(FormulaFunctionsText, a_text_is_counted_in_characters) {
  EXPECT_EQ(ods("=LEN(12.5)"), number(4));
  EXPECT_EQ(ods(R"(=LEN(""))"), number(0));
  EXPECT_EQ(ods("=LEN([.A3])"), number(0));
  EXPECT_EQ(ods(R"(=LEN("äb"))"), number(2));
}

TEST(FormulaFunctionsText, a_part_of_a_text) {
  EXPECT_EQ(ods(R"(=LEFT("abc"))"), text("a"));
  EXPECT_EQ(ods(R"(=LEFT("abc";5))"), text("abc"));
  EXPECT_EQ(ods("=LEFT(TRUE())"), text("1"));
  EXPECT_EQ(xlsx("=LEFT(TRUE)"), text("T"));
  EXPECT_EQ(ods(R"(=MID("abcdef";2;3))"), text("bcd"));
  EXPECT_EQ(ods(R"(=MID("abc";5;1))"), text(""));
  EXPECT_EQ(ods(R"(=RIGHT("abc";0))"), text(""));
  EXPECT_EQ(ods(R"(=RIGHT("abc";2))"), text("bc"));
  // LibreOffice: Err:502
  EXPECT_EQ(ods(R"(=LEFT("abc";-1))"), std::nullopt);
  EXPECT_EQ(ods(R"(=MID("abc";0;1))"), std::nullopt);
  EXPECT_EQ(ods(R"(=MID("abc";2;-1))"), std::nullopt);
  EXPECT_EQ(xlsx(R"(=LEFT("abc",-1))"), error(ErrorType::value));
}

TEST(FormulaFunctionsText, the_case_of_a_text) {
  EXPECT_EQ(ods(R"(=UPPER("aBc"))"), text("ABC"));
  EXPECT_EQ(ods(R"(=UPPER("ä"))"), text("Ä"));
  EXPECT_EQ(ods(R"(=LOWER("ŁÖ"))"), text("łö"));
  EXPECT_EQ(ods(R"(=UPPER("ß"))"), std::nullopt);
  EXPECT_EQ(ods(R"(=UPPER("ж"))"), std::nullopt);
}

TEST(FormulaFunctionsText, texts_join_and_repeat) {
  EXPECT_EQ(ods(R"(=TRIM("  a   b  "))"), text("a b"));
  EXPECT_EQ(ods(R"(=CONCATENATE("a";1;TRUE()))"), text("a11"));
  EXPECT_EQ(xlsx(R"(=CONCATENATE("a",1,TRUE))"), text("a1TRUE"));
  EXPECT_EQ(xlsx("=_xlfn.CONCAT(A1:A2)"), text("1abc"));
  EXPECT_EQ(ods(R"(=REPT("ab";2.9))"), text("abab"));
  EXPECT_EQ(ods(R"(=REPT("a";-1))"), std::nullopt);
}

TEST(FormulaFunctionsText, a_text_inside_a_text) {
  EXPECT_EQ(ods(R"(=FIND("b";"abcb";3))"), number(4));
  EXPECT_EQ(ods(R"(=FIND("x";"abc"))"), error(ErrorType::value));
  EXPECT_EQ(ods(R"(=FIND("a";"abc";0))"), error(ErrorType::value));
  EXPECT_EQ(ods(R"(=FIND("";"abc"))"), error(ErrorType::value));
  EXPECT_EQ(xlsx(R"(=FIND("","abc"))"), number(1));
  EXPECT_EQ(ods(R"(=SEARCH("B";"abc"))"), number(2));
  EXPECT_EQ(ods(R"(=SEARCH("a";"bAc";3))"), error(ErrorType::value));
  EXPECT_EQ(ods(R"(=SEARCH("b.";"abc"))"), std::nullopt);
  EXPECT_EQ(xlsx(R"(=SEARCH("b?","abc"))"), std::nullopt);
}

TEST(FormulaFunctionsText, a_text_replaces_a_part) {
  EXPECT_EQ(ods(R"(=SUBSTITUTE("aaa";"a";"b";2))"), text("aba"));
  EXPECT_EQ(ods(R"(=SUBSTITUTE("aaa";"a";"b"))"), text("bbb"));
  EXPECT_EQ(ods(R"(=SUBSTITUTE("abc";"";"x"))"), text("abc"));
  EXPECT_EQ(ods(R"(=REPLACE("abcdef";2;3;"X"))"), text("aXef"));
  EXPECT_EQ(ods(R"(=REPLACE("abc";5;1;"x"))"), text("abcx"));
}

TEST(FormulaFunctionsText, a_text_as_a_value) {
  EXPECT_EQ(ods(R"(=EXACT("a";"A"))"), boolean(false));
  EXPECT_EQ(ods(R"(=EXACT("a";"a"))"), boolean(true));
  EXPECT_EQ(ods(R"(=VALUE("1e3"))"), number(1000));
  EXPECT_EQ(ods(R"(=VALUE("abc"))"), std::nullopt);
  EXPECT_EQ(xlsx(R"(=VALUE("abc"))"), error(ErrorType::value));
  EXPECT_EQ(ods("=CHAR(65)"), text("A"));
  EXPECT_EQ(ods("=CHAR(200)"), std::nullopt);
  EXPECT_EQ(ods(R"(=CODE("A"))"), number(65));
}

TEST(FormulaEvaluator, a_latin_letter_compares_without_case) {
  Settings settings{.dialect = Dialect::libreoffice, .case_sensitive = false};
  EXPECT_EQ(evaluated(R"(="Ü"="ü")", Syntax::opendocument, settings),
            boolean(true));
  EXPECT_EQ(ods(R"(="Ü"="ü")"), boolean(false));
}
