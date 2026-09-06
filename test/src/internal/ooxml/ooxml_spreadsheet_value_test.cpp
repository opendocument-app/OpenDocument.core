#include <odr/document.hpp>
#include <odr/document_element.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

CellValue value_of(const std::string &sheet_data) {
  return first_sheet(decode(workbook(sheet_data))).cell(0, 0).value();
}

} // namespace

/// ECMA-376 18.3.1.4: `c/@t` defaults to "n", so a bare `<v>` is a number.
TEST(OoxmlSpreadsheetValue, a_number_cell_states_its_number) {
  const CellValue value =
      value_of(R"(<row r="1"><c r="A1"><v>12.5</v></c></row>)");

  EXPECT_EQ(value.type, ValueType::float_number);
  ASSERT_TRUE(value.number.has_value());
  EXPECT_DOUBLE_EQ(*value.number, 12.5);
  EXPECT_FALSE(value.formula.has_value());
}

TEST(OoxmlSpreadsheetValue, an_inline_string_cell_states_no_number) {
  const CellValue value = value_of(
      R"(<row r="1"><c r="A1" t="inlineStr"><is><t>12.5</t></is></c></row>)");

  EXPECT_EQ(value.type, ValueType::string);
  EXPECT_FALSE(value.number.has_value());
}

/// The `<v>` is the result the producer cached; `<f>` is what computed it.
TEST(OoxmlSpreadsheetValue, a_formula_cell_states_both_formula_and_result) {
  const CellValue value =
      value_of(R"(<row r="1"><c r="A1"><f>SUM(B1:C1)</f><v>7</v></c></row>)");

  EXPECT_EQ(value.type, ValueType::float_number);
  ASSERT_TRUE(value.number.has_value());
  EXPECT_DOUBLE_EQ(*value.number, 7);
  ASSERT_TRUE(value.formula.has_value());
  EXPECT_EQ(*value.formula, "SUM(B1:C1)");
}

/// Set-and-empty says the member computes; unset would claim it does not.
TEST(OoxmlSpreadsheetValue,
     a_shared_formula_member_holds_a_formula_it_cannot_spell) {
  const CellValue value = value_of(
      R"(<row r="1"><c r="A1"><f t="shared" si="0"/><v>8</v></c></row>)");

  ASSERT_TRUE(value.formula.has_value());
  EXPECT_TRUE(value.formula->empty());
}

/// `<v>` holds `1`, not a quantity, and `c/@t="b"` types the cell a string.
TEST(OoxmlSpreadsheetValue, a_boolean_cell_states_no_number) {
  const CellValue value =
      value_of(R"(<row r="1"><c r="A1" t="b"><v>1</v></c></row>)");

  EXPECT_EQ(value.type, ValueType::string);
  EXPECT_FALSE(value.number.has_value());
}
