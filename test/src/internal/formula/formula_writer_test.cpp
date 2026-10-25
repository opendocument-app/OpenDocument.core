#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_function_names.hpp>
#include <odr/internal/formula/formula_parser.hpp>
#include <odr/internal/formula/formula_writer.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
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

std::string edited(const std::string &text, const SheetEdit &edit,
                   const Syntax syntax = Syntax::ooxml) {
  std::optional<Node> node = parse(text, syntax);
  if (!node.has_value()) {
    return "<no parse>";
  }
  const bool moved = move_references(*node, edit, "here");
  return (moved ? "" : "unmoved ") + to_string(*node, syntax);
}

/// @p text after an edit of the rows of the formula's own sheet, or of the
/// one @p sheet names.
std::string edited(const std::string &text, const bool insert,
                   const std::uint32_t row, const std::uint32_t count,
                   const Syntax syntax = Syntax::ooxml,
                   const std::optional<std::string> &sheet = std::nullopt) {
  return edited(text,
                {.sheet = sheet.value_or("here"),
                 .index = row,
                 .count = count,
                 .insert = insert},
                syntax);
}

std::string inserted(const std::string &text, const std::uint32_t row,
                     const std::uint32_t count) {
  return edited(text, true, row, count);
}

std::string deleted(const std::string &text, const std::uint32_t row,
                    const std::uint32_t count) {
  return edited(text, false, row, count);
}

std::string columns(const std::string &text, const bool insert,
                    const std::uint32_t column, const std::uint32_t count) {
  return edited(text, {.sheet = "here",
                       .axis = Axis::column,
                       .index = column,
                       .count = count,
                       .insert = insert});
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
  EXPECT_EQ(ooxml("SUM(1:3)"), "SUM(1:3)");
  EXPECT_EQ(ooxml("SUM(Sheet1:Sheet3!A1)"), "SUM(Sheet1:Sheet3!A1)");
  EXPECT_EQ(ooxml("SUM('A B':'C D'!A1)"), "SUM('A B':'C D'!A1)");
  EXPECT_EQ(ooxml("SUM('A B:C D'!A1)"), "SUM('A B:C D'!A1)");
  EXPECT_EQ(ooxml("SUM('A''B':'C D'!A1)"), "SUM('A''B':'C D'!A1)");
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
  EXPECT_EQ(moved("B2", std::numeric_limits<std::int64_t>::max(), 0), "#REF!");
  EXPECT_EQ(moved("B2", 0, std::numeric_limits<std::int64_t>::min()), "#REF!");
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
  EXPECT_EQ(inserted("SUM(3:5)", 3, 1), "SUM(3:6)");
  EXPECT_EQ(inserted("SUM(A:B)", 0, 1), "unmoved SUM(A:B)");
}

TEST(FormulaWriter, a_row_edit_moves_only_the_edited_sheet) {
  EXPECT_EQ(inserted("Sheet2!A5+A5", 0, 1), "Sheet2!A5+A6");
  EXPECT_EQ(edited("SUM(SHEET2!A1:A5)", true, 0, 1, Syntax::ooxml, "Sheet2"),
            "SUM(SHEET2!A2:A6)");
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

TEST(FormulaWriter, a_row_edit_moves_each_address_of_a_list) {
  const SheetEdit insert{.sheet = "s", .index = 1, .count = 1};
  EXPECT_EQ(move_addresses("A1:B3 D5", insert, "s", Syntax::ooxml), "A1:B4 D6");
  EXPECT_EQ(move_addresses("A1", insert, "s", Syntax::ooxml), std::nullopt);
  EXPECT_EQ(move_addresses("$s.$A$1:.$A$3 $'a b'.A2", insert, "t",
                           Syntax::opendocument),
            "$s.$A$1:.$A$4 $'a b'.A2");

  const SheetEdit remove{.sheet = "s", .index = 1, .count = 1, .insert = false};
  EXPECT_EQ(move_addresses("A2 A3", remove, "s", Syntax::ooxml), "A2");
  EXPECT_EQ(move_addresses("A2", remove, "s", Syntax::ooxml), "");
}

TEST(FormulaWriter, an_inserted_column_moves_what_is_at_or_past_it) {
  EXPECT_EQ(columns("A1+C1+$C$1", true, 1, 2), "A1+E1+$E$1");
  EXPECT_EQ(columns("SUM(A1:C1)", true, 1, 2), "SUM(A1:E1)");
  EXPECT_EQ(columns("SUM(A:C)", true, 1, 1), "SUM(A:D)");
  EXPECT_EQ(columns("SUM($3:$5)", true, 0, 1), "unmoved SUM($3:$5)");
}

TEST(FormulaWriter, a_deleted_column_loses_or_shrinks_what_it_reaches) {
  EXPECT_EQ(columns("B1+C1", false, 1, 1), "#REF!+B1");
  EXPECT_EQ(columns("SUM(A1:C1)", false, 1, 1), "SUM(A1:B1)");
  EXPECT_EQ(columns("SUM(C:D)", false, 0, 1), "SUM(B:C)");
}

TEST(FormulaWriter, a_column_edit_moves_each_address_of_a_list) {
  const SheetEdit insert{
      .sheet = "s", .axis = Axis::column, .index = 1, .count = 1};
  EXPECT_EQ(move_addresses("A1:C3 E5", insert, "s", Syntax::ooxml), "A1:D3 F5");
}

// The names are the ones LibreOffice shows in each language.
TEST(FormulaWriter, a_function_name_takes_the_language_of_the_document) {
  EXPECT_EQ(localized_functions("SUM(A1;B1)*2", "de-DE"), "SUMME(A1;B1)*2");
  EXPECT_EQ(localized_functions("IF(A1,1,2)", "fr"), "SI(A1,1,2)");
  EXPECT_EQ(english_functions("SUMME(A1;B1)", "de-CH"), "SUM(A1;B1)");
  EXPECT_EQ(english_functions("ZÄHLENWENN (A1:A3;1)", "de"),
            "COUNTIF (A1:A3;1)");
  // only a name ahead of `(`, and none in a text or a quoted sheet name
  EXPECT_EQ(english_functions(R"(LEN("SUMME(")+'SUMME('!A1+SUMME)", "de"),
            R"(LEN("SUMME(")+'SUMME('!A1+SUMME)");
  // a language without a table keeps the English names
  EXPECT_EQ(localized_functions("SUM(A1)", "ja-JP"), "SUM(A1)");
  EXPECT_EQ(localized_functions("SUM(A1)", "no"),
            localized_functions("SUM(A1)", "nb"));
  // Brazil has its own table, whatever the tag states beside the region
  EXPECT_EQ(localized_functions("COLUMN(A1)", "pt-PT"), "COL(A1)");
  EXPECT_EQ(localized_functions("COLUMN(A1)", "pt-BR"), "COLUNA(A1)");
  EXPECT_EQ(localized_functions("COLUMN(A1)", "pt-Latn-BR"), "COLUNA(A1)");
  EXPECT_EQ(localized_functions("COLUMN(A1)", "pt_br"), "COLUNA(A1)");
}

TEST(FormulaWriter, localized_names_ignore_non_ascii_case) {
  EXPECT_EQ(english_functions("zÄhLeNwEnN(A1:A2,1)", "de-DE"),
            "COUNTIF(A1:A2,1)");
  EXPECT_EQ(english_functions("zählenwenn(A1:A2,1)", "de-DE"),
            "COUNTIF(A1:A2,1)");
  EXPECT_EQ(english_functions("сУмМ(A1:A2)", "ru-RU"), "SUM(A1:A2)");
  EXPECT_EQ(english_functions("dł(A1)", "pl-PL"), "LEN(A1)");
  EXPECT_EQ(english_functions("mínimo(A1:A2)", "pt-BR"), "MIN(A1:A2)");
  EXPECT_EQ(english_functions("min(A1:A2)", "tr-TR"), "MIN(A1:A2)");
  EXPECT_EQ(english_functions("yanlış()", "tr-TR"), "FALSE()");
  EXPECT_EQ(english_functions(R"(LEN("zählenwenn(")+'сумм('!A1)", "de-DE"),
            R"(LEN("zählenwenn(")+'сумм('!A1)");
  const std::string invalid =
      "Z" + std::string(1, static_cast<char>(0xFF)) + "(1)";
  EXPECT_EQ(english_functions(invalid, "de-DE"), invalid);
}
