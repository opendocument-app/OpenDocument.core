#include <internal/formula/formula_test_util.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <optional>
#include <string>

using namespace odr::internal::formula;
using namespace odr::test::formula;

// The results of `ods` are the ones LibreOffice computes for the formula
// over `criteria_cells()`, the results of `xlsx` the ones Excel documents.

namespace {

/// An `.ods` that states no settings: case-sensitive, with regular
/// expressions.
std::optional<Value> plain(const std::string &formula) {
  return ods(formula, criteria_cells());
}

/// An `.ods` as LibreOffice writes one now: wildcards, no regular
/// expressions, not case-sensitive.
std::optional<Value> current(const std::string &formula) {
  return evaluated(formula, Syntax::opendocument,
                   Settings{.dialect = Dialect::libreoffice,
                            .case_sensitive = false,
                            .wildcards = true,
                            .regular_expressions = false},
                   criteria_cells());
}

} // namespace

TEST(FormulaFunctionsLookup, a_text_criterion_ignores_case) {
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"Sp"))"), number(2));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"sp"))"), number(2));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"=Sp"))"), number(2));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"<>Sp"))"), number(12));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"Ü"))"), number(2));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"ab"))"), number(1));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"Sp "))"), number(1));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];" Sp"))"), number(0));
  EXPECT_EQ(current(R"(=COUNTIF([.A1:.A14];"Sp"))"), number(2));
}

TEST(FormulaFunctionsLookup, a_number_criterion_matches_a_text_spelling_it) {
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];">4"))"), number(1));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"5"))"), number(2));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"=5"))"), number(2));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"<>5"))"), number(12));
  EXPECT_EQ(plain("=COUNTIF([.A1:.A14];5)"), number(1));
  // the boolean is the number 1 in LibreOffice
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"<5"))"), number(1));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"1"))"), number(1));
  EXPECT_EQ(plain("=COUNTIF([.A1:.A14];1)"), number(1));
  EXPECT_EQ(plain("=COUNTIF([.A1:.A14];TRUE())"), number(1));
}

TEST(FormulaFunctionsLookup, an_empty_criterion_matches_by_its_operator) {
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];""))"), number(2));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"="))"), number(1));
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"<>"))"), number(13));
}

TEST(FormulaFunctionsLookup, a_pattern_criterion_has_no_answer) {
  // LibreOffice: 4 and 1 as regular expressions, 4 with wildcards
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"S.*"))"), std::nullopt);
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"S*"))"), std::nullopt);
  EXPECT_EQ(current(R"(=COUNTIF([.A1:.A14];"S*"))"), std::nullopt);
  // LibreOffice: 6 and 9, by its collator
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];">=Sp"))"), std::nullopt);
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];">a"))"), std::nullopt);
  // LibreOffice: 1 and 0
  EXPECT_EQ(plain(R"(=COUNTIF([.A1:.A14];"TRUE"))"), std::nullopt);
  EXPECT_EQ(plain("=COUNTIF([.A1:.A14];[.A8])"), std::nullopt);
}

TEST(FormulaFunctionsLookup, a_conditional_sum_and_average) {
  EXPECT_EQ(plain(R"(=SUMIF([.A1:.A14];">4"))"), number(5));
  EXPECT_EQ(plain(R"(=SUMIF([.A1:.A14];"Sp";[.A1:.A14]))"), number(0));
  EXPECT_EQ(current(R"(=SUMIFS([.A1:.A14];[.A1:.A14];">4"))"), number(5));
  EXPECT_EQ(current(R"(=AVERAGEIF([.A1:.A14];">4"))"), number(5));
  EXPECT_EQ(current(R"(=AVERAGEIF([.A1:.A14];">100"))"),
            error(ErrorType::division));
  EXPECT_EQ(current(R"(=COUNTIFS([.A1:.A14];"Sp";[.A1:.A14];"<>"))"),
            number(2));
}

TEST(FormulaFunctionsLookup, a_conditional_sum_adds_as_sum_does) {
  // LibreOffice's `KahanSum`; adding left to right gives 0.5
  EXPECT_EQ(plain("=SUMIF({1;1;1;1};1;{1;1E20;-1E20;0.5})"), number(1.5));
  EXPECT_EQ(xlsx("=SUMIF({1,1,1},1,{0.1,0.2,-0.3})"), std::nullopt);
}

TEST(FormulaFunctionsLookup, ranges_of_two_sizes_follow_the_dialect) {
  // LibreOffice: Err:502
  EXPECT_EQ(current(R"(=COUNTIFS([.A1:.A14];"Sp";[.A1:.A13];"<>"))"),
            std::nullopt);
  EXPECT_EQ(xlsx(R"(=COUNTIFS(A1:A6,"1",A1:A5,"<>"))"),
            error(ErrorType::value));
}

TEST(FormulaFunctionsLookup, an_exact_lookup_ignores_case) {
  EXPECT_EQ(current(R"(=MATCH("sp";[.A1:.A14];0))"), number(1));
  EXPECT_EQ(current(R"(=VLOOKUP("x sp";[.A1:.A14];1;0))"), text("x Sp"));
  EXPECT_EQ(current(R"(=MATCH("b";{"a";"B";"c"};0))"), number(2));
  EXPECT_EQ(current("=MATCH(TRUE();[.A1:.A14];0)"), number(7));
  EXPECT_EQ(current("=MATCH(1;[.A1:.A14];0)"), number(7));
  EXPECT_EQ(current("=MATCH(5;{1;2};0)"), error(ErrorType::not_available));
}

TEST(FormulaFunctionsLookup, a_table_lookup) {
  EXPECT_EQ(current(R"(=VLOOKUP(2;{1;"a"|2;"b"|3;"c"};2;0))"), text("b"));
  EXPECT_EQ(current(R"(=VLOOKUP(2.5;{1;"a"|2;"b"|3;"c"};2))"), text("b"));
  EXPECT_EQ(current(R"(=VLOOKUP(0;{1;"a"|2;"b"};2))"),
            error(ErrorType::not_available));
  EXPECT_EQ(current(R"(=HLOOKUP(2;{1;2;3|"a";"b";"c"};2;0))"), text("b"));
  // LibreOffice: Err:502
  EXPECT_EQ(current(R"(=VLOOKUP(4;{1;"a"|2;"b"};3;0))"), std::nullopt);
  EXPECT_EQ(current(R"(=VLOOKUP(1;{1;"a"};0;0))"), std::nullopt);
  EXPECT_EQ(xlsx(R"(=VLOOKUP(4,{1,"a";2,"b"},3,0))"),
            error(ErrorType::reference));
}

TEST(FormulaFunctionsLookup, an_approximate_lookup_needs_an_order) {
  EXPECT_EQ(current("=MATCH(3;{1;2;3;4};1)"), number(3));
  EXPECT_EQ(current("=MATCH(2.5;{1;2;3};1)"), number(2));
  // the binary search of each application decides on an unsorted line
  EXPECT_EQ(current("=MATCH(2;{3;1;2};1)"), std::nullopt);
  EXPECT_EQ(current("=MATCH(2;{1;2;2;3};1)"), std::nullopt);
}

TEST(FormulaFunctionsLookup, a_cell_by_its_position) {
  EXPECT_EQ(current("=INDEX({1;2|3;4};2;1)"), number(3));
  EXPECT_EQ(current("=INDEX([.A1:.A14];3)"), text("Sa"));
  EXPECT_EQ(current("=INDEX([.A1:.A14];20)"), error(ErrorType::reference));
  EXPECT_EQ(current("=ROW([.A3:.A5])"), number(3));
  EXPECT_EQ(current("=ROW()"), number(30));
  EXPECT_EQ(current("=COLUMN([.C2])"), number(3));
  EXPECT_EQ(current("=ROWS([.A1:.A14])"), number(14));
  EXPECT_EQ(current("=COLUMNS({1;2;3})"), number(3));
  // as tall as the grid, which the two applications state apart
  EXPECT_EQ(xlsx("=ROWS(A:A)"), std::nullopt);
}

TEST(FormulaFunctionsLookup, a_whole_column_past_the_extent_has_no_answer) {
  // the cells past the extent of the sheet are empty, and not read
  EXPECT_EQ(xlsx(R"(=COUNTIF(A:A,""))"), std::nullopt);
  EXPECT_EQ(xlsx(R"(=COUNTIF(A:A,"abc"))"), number(1));
  EXPECT_EQ(xlsx("=VLOOKUP(2,A:A,1)"), std::nullopt);
  EXPECT_EQ(xlsx("=INDEX(A:A,100)"), std::nullopt);
  // an exact match ends at the first hit, within the work limit of a lookup
  EXPECT_NE(xlsx(R"(=MATCH("abc",A:A,0))"), std::nullopt);
  EXPECT_EQ(xlsx(R"(=MATCH("abc",A:A,0))"), xlsx(R"(=MATCH("abc",A1:A14,0))"));
}

namespace {

/// `s!A1:C3` holds 1, 2, 3 / 10, 20, 30 / 100, 200, 300, and `t!A1` holds 7.
/// `Rate` names `s!C3`.
Cells grid() {
  Cells result;
  for (std::uint32_t row = 0; row < 3; ++row) {
    for (std::uint32_t column = 0; column < 3; ++column) {
      result.values[odr::SheetPosition(0, column, row)] =
          Value{(column + 1) * std::pow(10.0, row)};
    }
  }
  result.values[odr::SheetPosition(1, 0, 0)] = Value{7.0};
  result.names["rate"] = "[$s.$C$3]";
  return result;
}

} // namespace

TEST(FormulaFunctionsLookup, offset_moves_and_sizes_a_reference) {
  const Cells source = grid();
  EXPECT_EQ(ods("=SUM(OFFSET([.A1];1;1;2;2))", source), number(550));
  EXPECT_EQ(ods("=OFFSET([.A1];2;0)", source), number(100));
  EXPECT_EQ(ods("=OFFSET([.A1];1.9;0.9)", source), number(10));
  EXPECT_EQ(ods("=SUM(OFFSET([.A1:.B2];1;1))", source), number(550));
  EXPECT_EQ(ods("=SUM(OFFSET([.A1:.B2];0;0;1))", source), number(3));
  EXPECT_EQ(ods(R"(=SUM(OFFSET([.A1];"1";1)))", source), number(20));
  // a range the formula's row does not cross
  EXPECT_EQ(ods("=OFFSET([.A1];0;0;2;2)", source), error(ErrorType::value));
  EXPECT_EQ(xlsx("=SUM(OFFSET(A1,1,1,2,2))", source), number(550));
}

TEST(FormulaFunctionsLookup, offset_off_the_grid_is_refused) {
  const Cells source = grid();
  EXPECT_EQ(ods("=OFFSET([.A1];-1;0)", source), std::nullopt);
  EXPECT_EQ(xlsx("=OFFSET(A1,-1,0)", source), error(ErrorType::reference));
  EXPECT_EQ(xlsx("=OFFSET(A1,0,16384)", source), error(ErrorType::reference));
  // LibreOffice refuses a size below 1, and Excel does not document it
  EXPECT_EQ(ods("=SUM(OFFSET([.C3];0;0;-2;-2))", source), std::nullopt);
  EXPECT_EQ(ods("=SUM(OFFSET([.A1];0;0;0;1))", source), std::nullopt);
  EXPECT_EQ(xlsx("=SUM(OFFSET(A1,0,0,0,1))", source), std::nullopt);
}

TEST(FormulaFunctionsLookup, indirect_reads_a_reference_in_a_text) {
  const Cells source = grid();
  EXPECT_EQ(ods(R"(=INDIRECT("A2"))", source), number(10));
  EXPECT_EQ(ods(R"(=INDIRECT("a2"))", source), number(10));
  EXPECT_EQ(ods(R"(=INDIRECT("$A$2"))", source), number(10));
  EXPECT_EQ(ods(R"(=INDIRECT("A2:A2"))", source), number(10));
  EXPECT_EQ(ods(R"(=INDIRECT("A2";1))", source), number(10));
  EXPECT_EQ(ods(R"(=SUM(INDIRECT("A1:B2")))", source), number(33));
  EXPECT_EQ(ods(R"(=INDIRECT("Rate"))", source), number(300));
  EXPECT_EQ(xlsx(R"(=INDIRECT("A2"))", source), number(10));
  EXPECT_EQ(xlsx(R"(=INDIRECT("rate"))", source), number(300));
}

TEST(FormulaFunctionsLookup, indirect_reads_a_sheet_as_both_formats_spell_it) {
  const Cells source = grid();
  EXPECT_EQ(ods(R"(=INDIRECT("s!B2"))", source), number(20));
  EXPECT_EQ(ods(R"(=INDIRECT("'t'!A1"))", source), number(7));
  EXPECT_EQ(ods(R"(=SUM(INDIRECT("s!A1:B2")))", source), number(33));
  // LibreOffice reads its own spelling too, and Excel reads it as a name
  EXPECT_EQ(ods(R"(=INDIRECT("s.B2"))", source), number(20));
  EXPECT_EQ(ods(R"(=INDIRECT("$s.$B$2"))", source), number(20));
  EXPECT_EQ(ods(R"(=INDIRECT("$'t'.A1"))", source), number(7));
  EXPECT_EQ(ods(R"(=SUM(INDIRECT("s.A1:B2")))", source), number(33));
  EXPECT_EQ(ods(R"(=SUM(INDIRECT("s.A1:$B$2")))", source), number(33));
  EXPECT_EQ(xlsx(R"(=INDIRECT("s.B2"))", source), error(ErrorType::reference));
}

TEST(FormulaFunctionsLookup,
     indirect_reads_r1c1_where_its_second_argument_is_false) {
  const Cells source = grid();
  EXPECT_EQ(ods(R"(=INDIRECT("R2C2";0))", source), number(20));
  EXPECT_EQ(ods(R"(=INDIRECT("s!R2C2";0))", source), number(20));
  // relative to F30
  EXPECT_EQ(ods(R"(=INDIRECT("R[-29]C[-5]";0))", source), number(1));
  EXPECT_EQ(ods(R"(=INDIRECT("R2C[-4]";0))", source), number(20));
  EXPECT_EQ(ods(R"(=SUM(INDIRECT("R1C1:R2C2";0)))", source), number(33));
  EXPECT_EQ(xlsx(R"(=INDIRECT("R2C2",FALSE))", source), number(20));
  EXPECT_EQ(ods(R"(=INDIRECT("B2";0))", source), std::nullopt);
  // a whole row, which this does not read
  EXPECT_EQ(xlsx(R"(=INDIRECT("R2",FALSE))", source), std::nullopt);
}

TEST(FormulaFunctionsLookup, indirect_of_no_reference_is_ref) {
  const Cells source = grid();
  EXPECT_EQ(ods(R"(=INDIRECT("R2C2"))", source), error(ErrorType::reference));
  EXPECT_EQ(ods(R"(=INDIRECT("nonsense"))", source),
            error(ErrorType::reference));
  EXPECT_EQ(ods(R"(=INDIRECT(""))", source), error(ErrorType::reference));
  EXPECT_EQ(ods(R"(=INDIRECT(" A2"))", source), error(ErrorType::reference));
  EXPECT_EQ(ods(R"(=INDIRECT("x!A1"))", source), error(ErrorType::reference));
  EXPECT_EQ(ods("=INDIRECT([.A1])", source), error(ErrorType::reference));
  EXPECT_EQ(xlsx(R"(=INDIRECT("nonsense"))", source),
            error(ErrorType::reference));
  // whether Excel trims a space is not documented
  EXPECT_EQ(xlsx(R"(=INDIRECT(" A2"))", source), std::nullopt);
  // a space in a quoted sheet name is no space around the reference
  EXPECT_EQ(xlsx(R"(=INDIRECT("'x y'!A1"))", source),
            error(ErrorType::reference));
}
