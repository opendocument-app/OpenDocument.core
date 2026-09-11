#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_writer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <string>

using namespace odr::internal::formula;

namespace {

std::string round_trip(const std::string &text, const Syntax syntax) {
  const std::optional<Node> node = parse(text, syntax);
  return node.has_value() ? to_string(*node, syntax) : "<no parse>";
}

std::string odf(const std::string &text) {
  return round_trip(text, Syntax::opendocument);
}

std::string ooxml(const std::string &text) {
  return round_trip(text, Syntax::ooxml);
}

std::string moved(const std::string &text, const std::int64_t columns,
                  const std::int64_t rows) {
  std::optional<Node> node = parse(text, Syntax::ooxml);
  if (!node.has_value()) {
    return "<no parse>";
  }
  shift(*node, columns, rows);
  return to_string(*node, Syntax::ooxml);
}

} // namespace

TEST(FormulaWriter, an_ooxml_expression_is_written_as_it_was_read) {
  EXPECT_EQ(ooxml("SUM(A1:B2)"), "SUM(A1:B2)");
  EXPECT_EQ(ooxml("IF(A1>0,\"a\",\"b\")"), "IF(A1>0,\"a\",\"b\")");
  EXPECT_EQ(ooxml("$A$1+1"), "$A$1+1");
  EXPECT_EQ(ooxml("'My Sheet'!A1:B2"), "'My Sheet'!A1:B2");
  EXPECT_EQ(ooxml("[1]Sheet1!A1"), "[1]Sheet1!A1");
  EXPECT_EQ(ooxml("A1&\"x\""), "A1&\"x\"");
  EXPECT_EQ(ooxml("-3%"), "-3%");
  EXPECT_EQ(ooxml("{1,2;3,4}"), "{1,2;3,4}");
  EXPECT_EQ(ooxml("IF(A1,,B1)"), "IF(A1,,B1)");
  EXPECT_EQ(ooxml("#DIV/0!"), "#DIV/0!");
  EXPECT_EQ(ooxml("TRUE"), "TRUE");
  EXPECT_EQ(ooxml("A:A"), "A:A");
  EXPECT_EQ(ooxml("SUM((A1:A2,B1:B2))"), "SUM((A1:A2,B1:B2))");
  EXPECT_EQ(ooxml("[1]!Total"), "[1]!Total");
}

TEST(FormulaWriter, an_opendocument_expression_is_written_as_it_was_read) {
  EXPECT_EQ(odf("of:=SUM([.A1:.B2])"), "SUM([.A1:.B2])");
  EXPECT_EQ(odf("of:=[$'My Sheet'.$A$1]"), "[$'My Sheet'.$A$1]");
  EXPECT_EQ(odf("of:=IF([.A1]>0;1;2)"), "IF([.A1]>0;1;2)");
  EXPECT_EQ(odf("of:={1;2|3;4}"), "{1;2|3;4}");
  EXPECT_EQ(odf("of:=$$'Total Sales'"), "$$'Total Sales'");
  EXPECT_EQ(odf("of:=[.A1:Sheet2.B2]"), "[.A1:Sheet2.B2]");
  EXPECT_EQ(odf("of:=SUM([.A1:.A2]~[.B1:.B2])"), "SUM([.A1:.A2]~[.B1:.B2])");
  EXPECT_EQ(odf("of:=['file:///x.ods'#$Sheet1.A1]"),
            "['file:///x.ods'#$Sheet1.A1]");
  EXPECT_EQ(odf("of:=TRUE"), "TRUE()");
}

TEST(FormulaWriter, a_parenthesis_is_written_only_where_it_is_needed) {
  EXPECT_EQ(ooxml("(1+2)*3"), "(1+2)*3");
  EXPECT_EQ(ooxml("1+(2*3)"), "1+2*3");
  EXPECT_EQ(ooxml("1-(2-3)"), "1-(2-3)");
  EXPECT_EQ(ooxml("(1-2)-3"), "1-2-3");
  EXPECT_EQ(ooxml("-(1+2)"), "-(1+2)");
}

TEST(FormulaWriter, a_relative_reference_moves_and_an_absolute_one_does_not) {
  EXPECT_EQ(moved("A1+B1", 0, 1), "A2+B2");
  EXPECT_EQ(moved("$A$1+B1", 0, 1), "$A$1+B2");
  EXPECT_EQ(moved("A$1+$A1", 1, 1), "B$1+$A2");
  EXPECT_EQ(moved("SUM(A1:A5)", 2, 0), "SUM(C1:C5)");
  EXPECT_EQ(moved("Sheet2!A1", 0, 3), "Sheet2!A4");
}

TEST(FormulaWriter, a_reference_moved_off_the_grid_is_lost) {
  EXPECT_EQ(moved("A1+B2", 0, -1), "#REF!+B1");
  EXPECT_EQ(moved("SUM(A1:B2)", -1, 0), "SUM(#REF!)");
}

TEST(FormulaWriter, what_a_shift_does_not_name_is_left_alone) {
  EXPECT_EQ(moved("SUM(Total)+1", 3, 3), "SUM(Total)+1");
  EXPECT_EQ(moved("\"A1\"", 3, 3), "\"A1\"");
}
