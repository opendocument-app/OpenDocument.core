#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

/// Sheet `s` holds a table over B2:C4: a header row `X`, `Y` and two rows of
/// numbers, its `Y` column computed and sorted.
std::shared_ptr<internal::abstract::File> with_table() {
  internal::zip::ZipArchive zip;
  insert(
      zip, "[Content_Types].xml",
      R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
      R"(<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>)"
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
      R"(<sheets><sheet name="s" sheetId="1" r:id="rId1"/>)"
      R"(<sheet name="t" sheetId="2" r:id="rId2"/></sheets></workbook>)");
  insert(
      zip, "xl/_rels/workbook.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
      R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/>)"
      R"(</Relationships>)");
  insert(zip, "xl/worksheets/sheet2.xml",
         "<worksheet><sheetData/></worksheet>");
  insert(
      zip, "xl/styles.xml",
      R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"/>)");
  insert(
      zip, "xl/worksheets/sheet1.xml",
      R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
      R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
      R"(<sheetData>)"
      R"(<row r="2"><c r="B2" t="inlineStr"><is><t>X</t></is></c>)"
      R"(<c r="C2" t="inlineStr"><is><t>Y</t></is></c></row>)"
      R"(<row r="3"><c r="B3"><v>1</v></c><c r="C3"><f>B3*2</f><v>2</v></c></row>)"
      R"(<row r="4"><c r="B4"><v>2</v></c><c r="C4"><f>B4*2</f><v>4</v></c></row>)"
      R"(</sheetData><tableParts count="1"><tablePart r:id="rId1"/></tableParts>)"
      R"(</worksheet>)");
  insert(
      zip, "xl/worksheets/_rels/sheet1.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/table" Target="../tables/table1.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/tables/table1.xml",
      R"(<table xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
      R"(id="1" name="T" displayName="T" ref="B2:C4">)"
      R"(<autoFilter ref="B2:C4"/>)"
      R"(<sortState ref="B3:C4"><sortCondition ref="C3:C4"/></sortState>)"
      R"(<tableColumns count="2">)"
      R"(<tableColumn id="1" name="X"/><tableColumn id="2" name="Y">)"
      R"(<calculatedColumnFormula>B3*2</calculatedColumnFormula>)"
      R"(<totalsRowFormula>SUM(t!B3:B4)</totalsRowFormula></tableColumn>)"
      R"(</tableColumns></table>)");

  std::stringstream out;
  zip.save(out);
  return std::make_shared<internal::MemoryFile>(out.str());
}

std::string table_xml(const Document &document) {
  return saved_part(document, "/xl/tables/table1.xml");
}

bool contains(const std::string &xml, const std::string &part) {
  return xml.find(part) != std::string::npos;
}

} // namespace

TEST(OoxmlSpreadsheetTables, a_table_formula_can_read_another_sheet) {
  const Document document = decode(with_table());
  first_sheet(document).next_sibling().as_sheet().insert_rows(0, 2);
  const std::string xml = table_xml(document);
  EXPECT_TRUE(contains(xml, R"(ref="B2:C4")"));
  EXPECT_TRUE(contains(xml, "<calculatedColumnFormula>B3*2<"));
  EXPECT_TRUE(contains(xml, "<totalsRowFormula>SUM(t!B5:B6)<"));
}

TEST(OoxmlSpreadsheetTables, a_row_inside_a_table_grows_it) {
  const Document document = decode(with_table());

  first_sheet(document).insert_rows(3, 1);

  const std::string xml = table_xml(document);
  EXPECT_TRUE(contains(xml, R"(ref="B2:C5"><autoFilter ref="B2:C5"/>)"));
  EXPECT_TRUE(contains(xml, "<calculatedColumnFormula>B3*2<"));
}

TEST(OoxmlSpreadsheetTables, a_row_at_its_header_moves_it) {
  const Document document = decode(with_table());

  first_sheet(document).insert_rows(1, 2);

  const std::string xml = table_xml(document);
  EXPECT_TRUE(contains(xml, R"(ref="B4:C6")"));
  EXPECT_TRUE(contains(xml, "<calculatedColumnFormula>B5*2<"));
}

TEST(OoxmlSpreadsheetTables, a_delete_shrinks_it_and_refuses_its_header) {
  const Document document = decode(with_table());
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.delete_rows(1, 1), UnsupportedOperation);
  EXPECT_THROW(sheet.delete_columns(1, 2), UnsupportedOperation);

  sheet.delete_rows(2, 1);
  EXPECT_TRUE(contains(table_xml(document), R"(ref="B2:C3")"));
}

TEST(OoxmlSpreadsheetTables, a_column_inside_a_table_gets_a_named_column) {
  const Document document = decode(with_table());
  const Sheet sheet = first_sheet(document);

  sheet.insert_columns(2, 1);

  const std::string xml = table_xml(document);
  EXPECT_TRUE(contains(xml, R"(ref="B2:D4")"));
  EXPECT_TRUE(
      contains(xml, R"(<sortState ref="B3:D4"><sortCondition ref="D3:D4"/>)"));
  EXPECT_TRUE(
      contains(xml, R"(<tableColumns count="3"><tableColumn id="1" name="X"/>)"
                    R"(<tableColumn id="3" name="Column1"/>)"
                    R"(<tableColumn id="2" name="Y">)"));
  EXPECT_TRUE(contains(xml, "<calculatedColumnFormula>B3*2<"));
  // Excel repairs a table whose header cells differ from its column names
  EXPECT_EQ(sheet.cell(2, 1).value().text(), "Column1");
  EXPECT_EQ(sheet.cell(3, 1).value().text(), "Y");
}

TEST(OoxmlSpreadsheetTables, a_removed_column_loses_its_table_column) {
  const Document document = decode(with_table());

  first_sheet(document).delete_columns(1, 1);

  const std::string xml = table_xml(document);
  EXPECT_TRUE(contains(xml, R"(ref="B2:B4")"));
  EXPECT_TRUE(
      contains(xml, R"(<sortState ref="B3:B4"><sortCondition ref="B3:B4"/>)"));
  EXPECT_TRUE(contains(
      xml, R"(<tableColumns count="1"><tableColumn id="2" name="Y">)"));
  EXPECT_TRUE(contains(xml, "<calculatedColumnFormula>#REF!*2<"));
}
