#include <odr/internal/formula/formula_ast.hpp>
#include <odr/internal/formula/formula_parser.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <string>

using namespace odr::internal::formula;

namespace {

std::optional<Node> odf(const std::string &text) {
  return parse(text, Syntax::opendocument);
}

std::optional<Node> ooxml(const std::string &text) {
  return parse(text, Syntax::ooxml);
}

CellReference cell_of(const Node &node) { return node.get<CellReference>(); }

CellReference at(const std::uint32_t column, const std::uint32_t row) {
  CellReference reference;
  reference.column = Coordinate{column, false};
  reference.row = Coordinate{row, false};
  return reference;
}

} // namespace

TEST(FormulaParser, a_number_is_read_with_its_exponent) {
  const std::optional<Node> node = ooxml("1.25e-3");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<NumberLiteral>());
  EXPECT_DOUBLE_EQ(node->get<NumberLiteral>().value, 1.25e-3);
}

TEST(FormulaParser, a_doubled_quote_is_one_quote_in_a_string) {
  const std::optional<Node> node = ooxml(R"("a""b")");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<StringLiteral>());
  EXPECT_EQ(node->get<StringLiteral>().value, "a\"b");
}

TEST(FormulaParser, a_bare_word_is_a_boolean_where_it_names_one) {
  const std::optional<Node> node = ooxml("TRUE");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<BooleanLiteral>());
  EXPECT_TRUE(node->get<BooleanLiteral>().value);
}

TEST(FormulaParser, an_error_is_read_by_its_spelling) {
  const std::optional<Node> node = ooxml("#NAME?");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<ErrorLiteral>());
  EXPECT_EQ(node->get<ErrorLiteral>().type, ErrorType::name);
}

TEST(FormulaParser, an_ooxml_cell_reference_is_a_position) {
  const std::optional<Node> node = ooxml("B3");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<CellReference>());
  EXPECT_EQ(cell_of(*node), at(1, 2));
}

TEST(FormulaParser, the_dollars_of_a_reference_are_kept) {
  const std::optional<Node> node = ooxml("$B$3");

  ASSERT_TRUE(node.has_value());
  const CellReference reference = cell_of(*node);
  ASSERT_TRUE(reference.column.has_value());
  ASSERT_TRUE(reference.row.has_value());
  EXPECT_EQ(reference.column->index, 1);
  EXPECT_TRUE(reference.column->absolute);
  EXPECT_EQ(reference.row->index, 2);
  EXPECT_TRUE(reference.row->absolute);
}

TEST(FormulaParser, a_column_letter_is_read_without_case) {
  const std::optional<Node> node = ooxml("aa1");

  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(cell_of(*node), at(26, 0));
}

TEST(FormulaParser, an_ooxml_range_states_both_corners) {
  const std::optional<Node> node = ooxml("A1:B2");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<RangeReference>());
  EXPECT_EQ(node->get<RangeReference>().from, at(0, 0));
  EXPECT_EQ(node->get<RangeReference>().to, at(1, 1));
}

TEST(FormulaParser, a_whole_column_states_no_row) {
  const std::optional<Node> node = ooxml("A:A");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<RangeReference>());
  const RangeReference range = node->get<RangeReference>();
  ASSERT_TRUE(range.from.column.has_value());
  EXPECT_EQ(range.from.column->index, 0);
  EXPECT_FALSE(range.from.row.has_value());
  EXPECT_FALSE(range.to.row.has_value());
}

TEST(FormulaParser, an_ooxml_reference_takes_its_sheet_name) {
  const std::optional<Node> node = ooxml("'My Sheet'!A1:B2");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<RangeReference>());
  const RangeReference range = node->get<RangeReference>();
  ASSERT_TRUE(range.from.sheet.has_value());
  EXPECT_EQ(*range.from.sheet, "My Sheet");
  EXPECT_FALSE(range.to.sheet.has_value());
}

TEST(FormulaParser, an_external_ooxml_reference_states_its_document) {
  const std::optional<Node> node = ooxml("[1]Sheet1!A1");

  ASSERT_TRUE(node.has_value());
  const CellReference reference = cell_of(*node);
  ASSERT_TRUE(reference.document.has_value());
  EXPECT_EQ(*reference.document, "1");
  ASSERT_TRUE(reference.sheet.has_value());
  EXPECT_EQ(*reference.sheet, "Sheet1");
}

TEST(FormulaParser, a_reference_over_several_sheets_keeps_its_spelling) {
  const std::optional<Node> node = ooxml("Sheet1:Sheet3!A1");

  ASSERT_TRUE(node.has_value());
  const CellReference reference = cell_of(*node);
  ASSERT_TRUE(reference.sheet.has_value());
  EXPECT_EQ(*reference.sheet, "Sheet1:Sheet3");
}

TEST(FormulaParser, a_quoted_span_of_sheets_keeps_its_quotes) {
  const std::optional<Node> node = ooxml("'A B':'C D'!A1");

  ASSERT_TRUE(node.has_value());
  const CellReference reference = cell_of(*node);
  ASSERT_TRUE(reference.sheet.has_value());
  EXPECT_EQ(*reference.sheet, "'A B':'C D'");
}

TEST(FormulaParser, a_name_of_another_workbook_carries_no_sheet) {
  const std::optional<Node> node = ooxml("[1]!Total");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<NameReference>());
  const NameReference name = node->get<NameReference>();
  ASSERT_TRUE(name.document.has_value());
  EXPECT_EQ(*name.document, "1");
  EXPECT_FALSE(name.sheet.has_value());
  EXPECT_EQ(name.name, "Total");
}

/// `LOG10` reads as column `LOG`, row 10 until the `(` says it is a call.
TEST(FormulaParser, a_name_in_front_of_a_paren_is_a_function) {
  const std::optional<Node> node = ooxml("LOG10(100)");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<FunctionCall>());
  EXPECT_EQ(node->get<FunctionCall>().name, "LOG10");
  ASSERT_EQ(node->children.size(), 1);
}

TEST(FormulaParser, a_word_that_is_no_reference_is_a_name) {
  const std::optional<Node> node = ooxml("Sales");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<NameReference>());
  EXPECT_EQ(node->get<NameReference>().name, "Sales");
}

TEST(FormulaParser, an_omitted_argument_is_its_own_node) {
  const std::optional<Node> node = ooxml("IF(A1,,B1)");

  ASSERT_TRUE(node.has_value());
  ASSERT_EQ(node->children.size(), 3);
  EXPECT_TRUE(node->children[1].holds<Missing>());
}

TEST(FormulaParser, a_function_of_no_arguments_parses) {
  const std::optional<Node> node = ooxml("TODAY()");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<FunctionCall>());
  EXPECT_TRUE(node->children.empty());
}

TEST(FormulaParser, a_prefixed_function_name_keeps_its_dots) {
  const std::optional<Node> node = ooxml("_xlfn.FLOOR.MATH(A1)");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<FunctionCall>());
  EXPECT_EQ(node->get<FunctionCall>().name, "_xlfn.FLOOR.MATH");
}

TEST(FormulaParser, a_comma_inside_a_group_unites_two_ranges) {
  const std::optional<Node> node = ooxml("SUM((A1:A2,B1:B2))");

  ASSERT_TRUE(node.has_value());
  ASSERT_EQ(node->children.size(), 1);
  ASSERT_TRUE(node->children[0].holds<BinaryOperation>());
  EXPECT_EQ(node->children[0].get<BinaryOperation>().op, BinaryOperator::unite);
}

TEST(FormulaParser, an_array_states_its_shape) {
  const std::optional<Node> node = ooxml("{1,2;3,4}");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<ArrayLiteral>());
  EXPECT_EQ(node->get<ArrayLiteral>().columns, 2);
  EXPECT_EQ(node->get<ArrayLiteral>().rows, 2);
  EXPECT_EQ(node->children.size(), 4);
}

TEST(FormulaParser, a_product_binds_tighter_than_a_sum) {
  const std::optional<Node> node = ooxml("1+2*3");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<BinaryOperation>());
  EXPECT_EQ(node->get<BinaryOperation>().op, BinaryOperator::add);
  ASSERT_EQ(node->children.size(), 2);
  EXPECT_EQ(node->children[1].get<BinaryOperation>().op,
            BinaryOperator::multiply);
}

TEST(FormulaParser, a_power_binds_tighter_than_a_product) {
  const std::optional<Node> node = ooxml("2*3^2");

  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(node->get<BinaryOperation>().op, BinaryOperator::multiply);
  EXPECT_EQ(node->children[1].get<BinaryOperation>().op, BinaryOperator::power);
}

TEST(FormulaParser, a_sign_binds_tighter_than_a_power) {
  const std::optional<Node> node = ooxml("-2^2");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<BinaryOperation>());
  EXPECT_EQ(node->get<BinaryOperation>().op, BinaryOperator::power);
  EXPECT_TRUE(node->children[0].holds<UnaryOperation>());
}

TEST(FormulaParser, a_percent_binds_tighter_than_a_sign) {
  const std::optional<Node> node = ooxml("-3%");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<UnaryOperation>());
  EXPECT_EQ(node->get<UnaryOperation>().op, UnaryOperator::minus);
  EXPECT_EQ(node->children[0].get<UnaryOperation>().op, UnaryOperator::percent);
}

TEST(FormulaParser, a_comparison_is_the_outermost_operator) {
  const std::optional<Node> node = ooxml("A1+1<>B1&\"x\"");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<BinaryOperation>());
  EXPECT_EQ(node->get<BinaryOperation>().op, BinaryOperator::not_equal);
}

TEST(FormulaParser, space_between_tokens_is_filler) {
  const std::optional<Node> node = ooxml(" 1 + 2 ");

  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(node->get<BinaryOperation>().op, BinaryOperator::add);
}

TEST(FormulaParser, a_leading_equals_sign_is_dropped) {
  const std::optional<Node> node = ooxml("=A1");

  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(cell_of(*node), at(0, 0));
}

TEST(FormulaParser, what_does_not_parse_is_nothing) {
  EXPECT_FALSE(ooxml("SUM(").has_value());
  EXPECT_FALSE(ooxml("1 +").has_value());
  EXPECT_FALSE(ooxml("\"open").has_value());
  EXPECT_FALSE(ooxml("").has_value());
  EXPECT_FALSE(ooxml("A1 B1").has_value());
  EXPECT_FALSE(ooxml("1e999").has_value());
}

/// Rows are 1-based, and both axes stop at what an index holds.
TEST(FormulaParser, a_spelling_past_the_grid_is_a_name) {
  for (const std::string text : {"ABCDEFGHI1", "A99999999999", "A0"}) {
    const std::optional<Node> node = ooxml(text);
    ASSERT_TRUE(node.has_value()) << text;
    EXPECT_TRUE(node->holds<NameReference>()) << text;
  }
}

TEST(FormulaParser, the_namespace_prefix_of_a_table_formula_is_dropped) {
  const std::optional<Node> node = odf("of:=[.B3]");

  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(cell_of(*node), at(1, 2));
}

TEST(FormulaParser, an_older_producers_prefix_is_dropped_too) {
  const std::optional<Node> node = odf("oooc:=[.A1]");

  ASSERT_TRUE(node.has_value());
  EXPECT_EQ(cell_of(*node), at(0, 0));
}

TEST(FormulaParser, a_string_holding_the_prefix_spelling_survives) {
  const std::optional<Node> node = odf(R"(="a:=b")");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<StringLiteral>());
  EXPECT_EQ(node->get<StringLiteral>().value, "a:=b");
}

TEST(FormulaParser, an_odf_range_is_one_bracket) {
  const std::optional<Node> node = odf("of:=SUM([.A1:.B2])");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<FunctionCall>());
  EXPECT_EQ(node->get<FunctionCall>().name, "SUM");
  ASSERT_EQ(node->children.size(), 1);
  ASSERT_TRUE(node->children[0].holds<RangeReference>());
  EXPECT_EQ(node->children[0].get<RangeReference>().to, at(1, 1));
}

TEST(FormulaParser, an_odf_reference_takes_its_sheet_name) {
  const std::optional<Node> node = odf("of:=[$'My Sheet'.$A$1]");

  ASSERT_TRUE(node.has_value());
  const CellReference reference = cell_of(*node);
  EXPECT_TRUE(reference.sheet_absolute);
  ASSERT_TRUE(reference.sheet.has_value());
  EXPECT_EQ(*reference.sheet, "My Sheet");
}

TEST(FormulaParser, an_unquoted_odf_sheet_name_is_read) {
  const std::optional<Node> node = odf("of:=[Sheet2.A1]");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(cell_of(*node).sheet.has_value());
  EXPECT_EQ(*cell_of(*node).sheet, "Sheet2");
}

TEST(FormulaParser, the_second_corner_of_an_odf_range_may_name_a_sheet) {
  const std::optional<Node> node = odf("of:=[.A1:Sheet2.B2]");

  ASSERT_TRUE(node.has_value());
  const RangeReference range = node->get<RangeReference>();
  EXPECT_FALSE(range.from.sheet.has_value());
  ASSERT_TRUE(range.to.sheet.has_value());
  EXPECT_EQ(*range.to.sheet, "Sheet2");
}

TEST(FormulaParser, an_external_odf_reference_states_its_document) {
  const std::optional<Node> node = odf("of:=['file:///x.ods'#$Sheet1.A1]");

  ASSERT_TRUE(node.has_value());
  const CellReference reference = cell_of(*node);
  ASSERT_TRUE(reference.document.has_value());
  EXPECT_EQ(*reference.document, "file:///x.ods");
  ASSERT_TRUE(reference.sheet.has_value());
  EXPECT_EQ(*reference.sheet, "Sheet1");
}

TEST(FormulaParser, a_lost_odf_reference_is_an_error) {
  const std::optional<Node> node = odf("of:=[#REF!]");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<ErrorLiteral>());
  EXPECT_EQ(node->get<ErrorLiteral>().type, ErrorType::reference);
}

TEST(FormulaParser, an_odf_function_separates_its_arguments_with_semicolons) {
  const std::optional<Node> node = odf("of:=IF([.A1]>0;\"a\";\"b\")");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<FunctionCall>());
  EXPECT_EQ(node->children.size(), 3);
}

TEST(FormulaParser, an_odf_named_expression_carries_two_dollars) {
  const std::optional<Node> node = odf("of:=$$'Total Sales'");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<NameReference>());
  EXPECT_EQ(node->get<NameReference>().name, "Total Sales");
}

TEST(FormulaParser, an_odf_array_separates_its_rows_with_bars) {
  const std::optional<Node> node = odf("of:={1;2|3;4}");

  ASSERT_TRUE(node.has_value());
  ASSERT_TRUE(node->holds<ArrayLiteral>());
  EXPECT_EQ(node->get<ArrayLiteral>().columns, 2);
  EXPECT_EQ(node->get<ArrayLiteral>().rows, 2);
}

TEST(FormulaParser, the_odf_reference_operators_parse) {
  const std::optional<Node> node = odf("of:=SUM([.A1:.A2]~[.B1:.B2])");

  ASSERT_TRUE(node.has_value());
  ASSERT_EQ(node->children.size(), 1);
  EXPECT_EQ(node->children[0].get<BinaryOperation>().op, BinaryOperator::unite);
}
