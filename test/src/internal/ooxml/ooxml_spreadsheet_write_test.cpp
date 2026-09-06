#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <string>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

constexpr const char *two_shared = R"(<row r="1"><c r="A1" t="s"><v>0</v></c>)"
                                   R"(<c r="B1" t="s"><v>0</v></c></row>)";
constexpr const char *one_string = R"(<si><t>same</t></si>)";

} // namespace

TEST(OoxmlSpreadsheetWrite, a_string_lands_in_the_cell_it_was_written_to) {
  const Document document = decode(workbook(
      R"(<row r="1"><c r="A1" t="inlineStr"><is><t>old</t></is></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 0, CellValue("new"));

  EXPECT_EQ(sheet.cell(0, 0).value().text(), "new");
  EXPECT_EQ(sheet.cell(0, 0).value().type(), ValueType::string);
}

TEST(OoxmlSpreadsheetWrite, a_number_lands_as_a_number) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>1</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 0, CellValue(12.5, "12.5"));

  const CellValue value = sheet.cell(0, 0).value();
  EXPECT_EQ(value.type(), ValueType::float_number);
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 12.5);
  EXPECT_EQ(sheet.cell(0, 0).value().text(), "12.5");
}

/// The point of writing an inline string rather than a shared one: two cells
/// index the same `si`, and editing that entry would edit both.
TEST(OoxmlSpreadsheetWrite,
     writing_a_shared_string_leaves_the_other_cell_alone) {
  const Document document = decode(workbook(two_shared, "", one_string));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 0, CellValue("mine"));

  EXPECT_EQ(sheet.cell(0, 0).value().text(), "mine");
  EXPECT_EQ(sheet.cell(1, 0).value().text(), "same");
}

TEST(OoxmlSpreadsheetWrite, a_cleared_cell_states_nothing) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>7</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.clear_cell(0, 0);

  const CellValue value = sheet.cell(0, 0).value();
  EXPECT_FALSE(value.has_number());
  EXPECT_EQ(sheet.cell(0, 0).value().text(), "");
}

TEST(OoxmlSpreadsheetWrite, a_formula_cell_refuses_to_be_written) {
  const Document document = decode(
      workbook(R"(<row r="1"><c r="A1"><f>SUM(B1:C1)</f><v>7</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(0, 0, CellValue("x")), UnsupportedOperation);
}

/// The anchor of the merge is the cell that holds the value.
TEST(OoxmlSpreadsheetWrite, a_covered_cell_refuses_to_be_written) {
  const Document document = decode(
      workbook(R"(<row r="1"><c r="A1" t="inlineStr"><is><t>a</t></is></c>)"
               R"(<c r="B1" t="inlineStr"><is><t>b</t></is></c></row>)",
               R"(<mergeCells><mergeCell ref="A1:B1"/></mergeCells>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(1, 0, CellValue("x")), UnsupportedOperation);
}

TEST(OoxmlSpreadsheetWrite, an_absent_cell_refuses_to_be_written) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>1</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(4, 4, CellValue("x")), UnsupportedOperation);
}

TEST(OoxmlSpreadsheetWrite, a_written_workbook_saves_and_reopens) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>1</v></c></row>)"));
  ASSERT_TRUE(document.is_editable());
  ASSERT_TRUE(document.is_savable());

  first_sheet(document).set_cell(0, 0, CellValue(41.5, "41.5"));

  std::ostringstream saved;
  document.save(saved);

  const Document reopened =
      open(File::from_memory(saved.str())).as_document_file().document();
  const CellValue value = first_sheet(reopened).cell(0, 0).value();

  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 41.5);
}

/// ECMA-376 18.2.2: nothing here computes a formula, so the reader is asked to.
TEST(OoxmlSpreadsheetWrite, a_saved_workbook_asks_to_be_recalculated) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>1</v></c></row>)"));

  std::ostringstream saved;
  document.save(saved);

  const Document reopened =
      open(File::from_memory(saved.str())).as_document_file().document();
  std::ostringstream workbook_xml;
  workbook_xml
      << reopened.as_filesystem().open("/xl/workbook.xml").stream()->rdbuf();

  EXPECT_NE(workbook_xml.str().find(R"(fullCalcOnLoad="1")"),
            std::string::npos);
}
