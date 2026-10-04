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

/// @p text after an edit of the rows of the formula's own sheet, or of the
/// one @p sheet names.
std::string edited(const std::string &text, const bool insert,
                   const std::uint32_t row, const std::uint32_t count,
                   const Syntax syntax = Syntax::ooxml,
                   const std::optional<std::string> &sheet = std::nullopt) {
  std::optional<Node> node = parse(text, syntax);
  if (!node.has_value()) {
    return "<no parse>";
  }
  const EditedSheet named = [&](const std::optional<std::string> &name) {
    return name == sheet;
  };
  const bool moved = insert ? insert_rows(*node, named, row, count)
                            : delete_rows(*node, named, row, count);
  return (moved ? "" : "unmoved ") + to_string(*node, syntax);
}

std::string inserted(const std::string &text, const std::uint32_t row,
                     const std::uint32_t count) {
  return edited(text, true, row, count);
}

std::string deleted(const std::string &text, const std::uint32_t row,
                    const std::uint32_t count) {
  return edited(text, false, row, count);
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

TEST(FormulaWriter, an_inserted_row_moves_what_is_at_or_past_it) {
  EXPECT_EQ(inserted("A1+A5", 2, 2), "A1+A7");
  EXPECT_EQ(inserted("$A$5+A$3", 2, 2), "$A$7+A$5");
  EXPECT_EQ(inserted("A1+A2", 2, 2), "unmoved A1+A2");
}

TEST(FormulaWriter, an_inserted_row_grows_a_range_it_falls_inside) {
  EXPECT_EQ(inserted("SUM(A1:A5)", 2, 2), "SUM(A1:A7)");
  EXPECT_EQ(inserted("SUM(A1:A5)", 0, 2), "SUM(A3:A7)");
  EXPECT_EQ(inserted("SUM(A1:A5)", 5, 2), "unmoved SUM(A1:A5)");
  EXPECT_EQ(inserted("SUM(A5:A1)", 2, 1), "SUM(A6:A1)");
}

TEST(FormulaWriter, a_row_edit_moves_whole_rows_and_leaves_whole_columns) {
  EXPECT_EQ(inserted("SUM($3:$5)", 3, 1), "SUM($3:$6)");
  EXPECT_EQ(inserted("SUM(A:B)", 0, 1), "unmoved SUM(A:B)");
}

TEST(FormulaWriter, a_row_edit_moves_only_the_edited_sheet) {
  EXPECT_EQ(inserted("Sheet2!A5+A5", 0, 1), "Sheet2!A5+A6");
  EXPECT_EQ(edited("Sheet2!A5+A5", true, 0, 1, Syntax::ooxml, "Sheet2"),
            "Sheet2!A6+A5");
  EXPECT_EQ(edited("[1]Sheet2!A5", true, 0, 1, Syntax::ooxml, "Sheet2"),
            "unmoved [1]Sheet2!A5");
  EXPECT_EQ(edited("SUM(Sheet2!A1:A5)", true, 0, 1, Syntax::ooxml, "Sheet2"),
            "SUM(Sheet2!A2:A6)");
  EXPECT_EQ(inserted("SUM(Total)", 0, 1), "unmoved SUM(Total)");
}

TEST(FormulaWriter, a_row_edit_moves_an_opendocument_reference) {
  EXPECT_EQ(edited("of:=SUM([.A1:.A5])", true, 1, 1, Syntax::opendocument),
            "SUM([.A1:.A6])");
  EXPECT_EQ(
      edited("of:=[$Sheet2.$A$5]", false, 0, 2, Syntax::opendocument, "Sheet2"),
      "[$Sheet2.$A$3]");
}

TEST(FormulaWriter, a_deleted_row_moves_what_is_past_it) {
  EXPECT_EQ(deleted("A1+A5", 1, 2), "A1+A3");
  EXPECT_EQ(deleted("A1", 1, 2), "unmoved A1");
}

TEST(FormulaWriter, a_deleted_row_loses_a_reference_all_inside_it) {
  EXPECT_EQ(deleted("A2+1", 1, 2), "#REF!+1");
  EXPECT_EQ(deleted("SUM(A2:A3)", 1, 2), "SUM(#REF!)");
  EXPECT_EQ(deleted("SUM($2:$3)", 1, 2), "SUM(#REF!)");
}

TEST(FormulaWriter, a_deleted_row_shrinks_a_range_it_cuts) {
  EXPECT_EQ(deleted("SUM(A1:A5)", 1, 2), "SUM(A1:A3)");
  EXPECT_EQ(deleted("SUM(A3:A6)", 1, 2), "SUM(A2:A4)");
  EXPECT_EQ(deleted("SUM(A1:A3)", 1, 2), "SUM(A1:A1)");
  EXPECT_EQ(deleted("SUM((A2):A5)", 1, 2), "SUM(A2:A3)");
}
