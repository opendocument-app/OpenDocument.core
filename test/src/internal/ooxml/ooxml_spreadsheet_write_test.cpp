#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>
#include <odr/table_dimension.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <string>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

/// What the sheet holds after a save, which is where the order of the rows and
/// the cells shows.
std::string worksheet_of(const Document &document) {
  return saved_part(document, "/xl/worksheets/sheet1.xml");
}

constexpr const char *two_shared = R"(<row r="1"><c r="A1" t="s"><v>0</v></c>)"
                                   R"(<c r="B1" t="s"><v>0</v></c></row>)";
constexpr const char *one_string = R"(<si><t>same</t></si>)";

} // namespace

TEST(OoxmlSpreadsheetWrite, a_string_lands_in_the_cell_it_was_written_to) {
  const Document document = decode(workbook(
      R"(<row r="1"><c r="A1" t="inlineStr"><is><t>old</t></is></c></row>)"));
  const Sheet sheet = first_sheet(document);

  const Text old = sheet.cell(0, 0).first_child().as_text();
  ASSERT_TRUE(old);
  sheet.set_cell(0, 0, CellValue("new"));

  EXPECT_THROW((void)old.content(), std::out_of_range);
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

/// A write states one inline string over whatever the cell held, so several
/// `r` runs go with it.
TEST(OoxmlSpreadsheetWrite, a_cell_of_several_runs_is_written) {
  const Document document = decode(
      workbook(R"(<row r="1"><c r="A1" t="inlineStr"><is>)"
               R"(<r><t>two </t></r><r><t>runs</t></r></is></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 0, CellValue("one"));

  EXPECT_EQ(sheet.cell(0, 0).value().text(), "one");
  EXPECT_EQ(worksheet_of(document).find("<r>"), std::string::npos);
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

TEST(OoxmlSpreadsheetWrite, a_boolean_lands_as_a_boolean) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>1</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 0, CellValue(ValueType::boolean).with_number(0));

  const CellValue value = sheet.cell(0, 0).value();
  EXPECT_EQ(value.type(), ValueType::boolean);
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 0);
  EXPECT_NE(worksheet_of(document).find(R"(<c r="A1" t="b"><v>0</v></c>)"),
            std::string::npos);
}

/// A refusal leaves the cell as it was.
TEST(OoxmlSpreadsheetWrite, an_error_or_a_date_without_a_number_refuses) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>1</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  for (const ValueType type :
       {ValueType::date, ValueType::time, ValueType::error}) {
    EXPECT_THROW(sheet.set_cell(0, 0, CellValue(type).with_text("x")),
                 UnsupportedOperation);
  }
  EXPECT_DOUBLE_EQ(sheet.cell(0, 0).value().number(), 1);
}

TEST(OoxmlSpreadsheetWrite, a_value_written_over_a_formula_takes_it_away) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><f>SUM(C1:D1)</f><v>7</v></c>)"
                      R"(<c r="B1"><f>A1*2</f><v>14</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 0, CellValue(5, "5"));

  EXPECT_FALSE(sheet.cell(0, 0).value().has_formula());
  (void)document.recalculate();
  EXPECT_DOUBLE_EQ(sheet.cell(1, 0).value().number(), 10);
}

/// ECMA-376 18.3.1.40: `f` comes ahead of `v`, which a recalculation writes.
TEST(OoxmlSpreadsheetWrite, a_formula_is_written_and_computed) {
  const Document document = decode(workbook(
      R"(<row r="1"><c r="A1"><v>3</v></c><c r="B1"><f>C1+1</f></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(2, 0, CellValue().with_formula("A1*2"));

  EXPECT_EQ(sheet.cell(2, 0).value().formula(), "A1*2");
  (void)document.recalculate();
  EXPECT_DOUBLE_EQ(sheet.cell(2, 0).value().number(), 6);
  EXPECT_DOUBLE_EQ(sheet.cell(1, 0).value().number(), 7);
  EXPECT_NE(worksheet_of(document).find(R"(<c r="C1"><f>A1*2</f><v>6</v></c>)"),
            std::string::npos)
      << worksheet_of(document);
}

TEST(OoxmlSpreadsheetWrite, a_formula_is_written_without_its_equals_sign) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>3</v></c></row>)"));

  first_sheet(document).set_cell(1, 0, CellValue().with_formula("=A1*2"));

  EXPECT_NE(worksheet_of(document).find(R"(<c r="B1"><f>A1*2</f><v>6</v></c>)"),
            std::string::npos)
      << worksheet_of(document);
}

TEST(OoxmlSpreadsheetWrite, a_formula_that_does_not_parse_is_refused) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>1</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(0, 0, CellValue().with_formula("SUM(")),
               UnsupportedOperation);
  EXPECT_DOUBLE_EQ(sheet.cell(0, 0).value().number(), 1);
}

/// ECMA-376 18.3.1.40: the other cells of a shared group and of an array read
/// their expression from the cell that states `ref`.
TEST(OoxmlSpreadsheetWrite, the_master_of_a_shared_formula_is_refused) {
  const Document document = decode(workbook(
      R"(<row r="1"><c r="A1"><f t="shared" ref="A1:B1" si="0">C1+1</f>)"
      R"(<v>1</v></c><c r="B1"><f t="shared" si="0"/><v>1</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(0, 0, CellValue("x")), UnsupportedOperation);
  sheet.set_cell(1, 0, CellValue("y"));
  EXPECT_EQ(sheet.cell(1, 0).value().text(), "y");
}

/// ECMA-376 18.6.1: Excel repairs a workbook whose calc chain names a cell that
/// holds no formula.
namespace {

/// A1 and B1 are formulas, and @p chain is the calc chain of the workbook.
std::shared_ptr<internal::abstract::File>
package_with_calc_chain(const std::string &chain) {
  internal::zip::ZipArchive zip;
  insert(
      zip, "[Content_Types].xml",
      R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
      R"(<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>)"
      R"(<Override PartName="/xl/calcChain.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.calcChain+xml"/>)"
      R"(</Types>)");
  insert(
      zip, "_rels/.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/workbook.xml",
      R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
      R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
      R"(<sheets><sheet name="s" sheetId="3" r:id="rId1"/></sheets></workbook>)");
  insert(
      zip, "xl/_rels/workbook.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
      R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/calcChain" Target="calcChain.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/styles.xml",
      R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"/>)");
  insert(
      zip, "xl/worksheets/sheet1.xml",
      R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"><sheetData>)"
      R"(<row r="1"><c r="A1"><f>1+1</f><v>2</v></c><c r="B1"><f>A1+1</f><v>3</v></c></row>)"
      R"(</sheetData></worksheet>)");
  insert(zip, "xl/calcChain.xml", chain);
  std::stringstream out;
  zip.save(out);
  return std::make_shared<internal::MemoryFile>(out.str());
}

} // namespace

TEST(OoxmlSpreadsheetWrite, a_value_over_a_formula_leaves_the_calc_chain) {
  const Document document = decode(package_with_calc_chain(
      R"(<calcChain xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)"
      R"(<c r="A1" i="3"/><c r="B1"/></calcChain>)"));

  first_sheet(document).set_cell(0, 0, CellValue(5, "5"));

  const std::string chain = saved_part(document, "/xl/calcChain.xml");
  EXPECT_EQ(chain.find(R"(r="A1")"), std::string::npos) << chain;
  // the entry after it takes the sheet the removed one named
  EXPECT_NE(chain.find(R"(<c i="3" r="B1"/>)"), std::string::npos) << chain;
}

/// ECMA-376 18.6.1: a `calcChain` holds one `c` at least.
TEST(OoxmlSpreadsheetWrite, a_calc_chain_without_a_cell_goes) {
  const Document document = decode(package_with_calc_chain(
      R"(<calcChain xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)"
      R"(<c r="A1" i="3"/></calcChain>)"));

  first_sheet(document).set_cell(0, 0, CellValue(5, "5"));

  const Document saved = saved_document(document);
  EXPECT_FALSE(saved.as_filesystem().is_file("/xl/calcChain.xml"));
  std::ostringstream relationships;
  relationships << saved.as_filesystem()
                       .open("/xl/_rels/workbook.xml.rels")
                       .stream()
                       ->rdbuf();
  EXPECT_EQ(relationships.str().find("calcChain"), std::string::npos)
      << relationships.str();
  std::ostringstream types;
  types << saved.as_filesystem().open("/[Content_Types].xml").stream()->rdbuf();
  EXPECT_EQ(types.str().find("calcChain"), std::string::npos) << types.str();
  EXPECT_DOUBLE_EQ(first_sheet(saved).cell(1, 0).value().number(), 6);
}

TEST(OoxmlSpreadsheetWrite, a_row_delete_that_empties_the_calc_chain_drops_it) {
  const Document document = decode(package_with_calc_chain(
      R"(<calcChain xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)"
      R"(<c r="A1" i="3"/><c r="B1"/></calcChain>)"));

  first_sheet(document).delete_rows(0, 1);

  EXPECT_FALSE(
      saved_document(document).as_filesystem().is_file("/xl/calcChain.xml"));
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

/// A cell the file states no `c` for is written by stating one.
TEST(OoxmlSpreadsheetWrite, an_absent_cell_is_written) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>1</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(4, 4, CellValue("x"));

  EXPECT_EQ(sheet.cell(4, 4).value().text(), "x");
  EXPECT_EQ(sheet.dimensions().columns, 5);
  EXPECT_EQ(sheet.dimensions().rows, 5);
}

/// 18.3.1.73 states the cells of a row in column order.
TEST(OoxmlSpreadsheetWrite, an_inserted_cell_lands_in_column_order) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="C1"><v>3</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 0, CellValue("a"));
  sheet.set_cell(1, 0, CellValue("b"));

  EXPECT_EQ(sheet.cell(0, 0).value().text(), "a");
  EXPECT_EQ(sheet.cell(1, 0).value().text(), "b");
  EXPECT_DOUBLE_EQ(sheet.cell(2, 0).value().number(), 3);

  const std::string xml = worksheet_of(document);
  EXPECT_LT(xml.find(R"(r="A1")"), xml.find(R"(r="B1")"));
  EXPECT_LT(xml.find(R"(r="B1")"), xml.find(R"(r="C1")"));
}

/// 18.3.1.80 states the rows of a sheet in row order.
TEST(OoxmlSpreadsheetWrite, an_inserted_row_lands_in_row_order) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>1</v></c></row>)"
                      R"(<row r="3"><c r="A3"><v>3</v></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 1, CellValue("two"));

  EXPECT_EQ(sheet.cell(0, 1).value().text(), "two");

  const std::string xml = worksheet_of(document);
  EXPECT_LT(xml.find(R"(r="A1")"), xml.find(R"(r="A2")"));
  EXPECT_LT(xml.find(R"(r="A2")"), xml.find(R"(r="A3")"));
}

/// 18.3.1.35 states the range the cells span, so a new cell widens it.
TEST(OoxmlSpreadsheetWrite, an_inserted_cell_widens_the_dimension) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="B2"><v>1</v></c></row>)", "", "", "",
                      R"(<dimension ref="B2:B2"/>)"));

  first_sheet(document).set_cell(3, 3, CellValue("x"));

  EXPECT_NE(worksheet_of(document).find(R"(ref="B2:D4")"), std::string::npos);
}

/// The anchor answers for the whole range, whether or not the file states a
/// `c` for the position covered.
TEST(OoxmlSpreadsheetWrite, an_absent_covered_cell_refuses_to_be_written) {
  const Document document = decode(
      workbook(R"(<row r="1"><c r="A1" t="inlineStr"><is><t>a</t></is></c>)"
               R"(</row>)",
               R"(<mergeCells><mergeCell ref="A1:B1"/></mergeCells>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(1, 0, CellValue("x")), UnsupportedOperation);
}

/// A `ref` bigger than the cells the sheet states is resolved by walking the
/// cells, and its last row and column are covered like any other.
TEST(OoxmlSpreadsheetWrite, the_last_cell_of_a_wide_merge_refuses_too) {
  const Document document = decode(
      workbook(R"(<row r="1"><c r="A1" t="inlineStr"><is><t>a</t></is></c>)"
               R"(<c r="C1" t="inlineStr"><is><t>c</t></is></c></row>)",
               R"(<mergeCells><mergeCell ref="A1:C1"/></mergeCells>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(2, 0, CellValue("x")), UnsupportedOperation);
}

TEST(OoxmlSpreadsheetWrite, an_inserted_cell_saves_and_reopens) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1"><v>1</v></c></row>)"));
  first_sheet(document).set_cell(2, 3, CellValue(41.5, "41.5"));

  std::ostringstream saved;
  document.save(saved);
  const Document reopened =
      open(File::from_memory(saved.str())).as_document_file().document();
  const Sheet sheet = first_sheet(reopened);

  ASSERT_TRUE(sheet.cell(2, 3).value().has_number());
  EXPECT_DOUBLE_EQ(sheet.cell(2, 3).value().number(), 41.5);
  EXPECT_DOUBLE_EQ(sheet.cell(0, 0).value().number(), 1);
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

/// ECMA-376 18.2.27 orders the `workbook` children, and `extLst` comes after
/// `calcPr`: appending the new one would put it on the wrong side.
TEST(OoxmlSpreadsheetWrite, a_new_calc_pr_lands_where_the_schema_orders_it) {
  const Document document = decode(workbook(
      R"(<row r="1"><c r="A1"><v>1</v></c></row>)", "", "", R"(<extLst/>)"));

  std::ostringstream saved;
  document.save(saved);

  const Document reopened =
      open(File::from_memory(saved.str())).as_document_file().document();
  std::ostringstream workbook_xml;
  workbook_xml
      << reopened.as_filesystem().open("/xl/workbook.xml").stream()->rdbuf();

  EXPECT_LT(workbook_xml.str().find("<calcPr"),
            workbook_xml.str().find("<extLst"));
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
