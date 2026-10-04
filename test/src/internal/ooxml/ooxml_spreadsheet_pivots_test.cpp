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

/// Sheet `s` with a pivot table at D10:E12 over @p source, A1:B3 by default.
std::shared_ptr<internal::abstract::File> with_pivot(
    const std::string &source = R"(<worksheetSource ref="A1:B3" sheet="s"/>)") {
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
      R"(<sheets><sheet name="s" sheetId="1" r:id="rId1"/></sheets>)"
      R"(<pivotCaches><pivotCache cacheId="1" r:id="rId2"/></pivotCaches></workbook>)");
  insert(
      zip, "xl/_rels/workbook.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
      R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/pivotCacheDefinition" Target="pivotCache/pivotCacheDefinition1.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/styles.xml",
      R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"/>)");
  insert(
      zip, "xl/worksheets/sheet1.xml",
      R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)"
      R"(<sheetData><row r="1"><c r="A1"><v>1</v></c></row></sheetData></worksheet>)");
  insert(
      zip, "xl/worksheets/_rels/sheet1.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/pivotTable" Target="../pivotTables/pivotTable1.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/pivotTables/pivotTable1.xml",
      R"(<pivotTableDefinition xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
      R"(name="P" cacheId="1"><location ref="D10:E12" firstHeaderRow="1")"
      R"( firstDataRow="1" firstDataCol="1"/></pivotTableDefinition>)");
  insert(
      zip, "xl/pivotCache/pivotCacheDefinition1.xml",
      R"(<pivotCacheDefinition xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)"
      R"(<cacheSource type="worksheet">)" +
          source + R"(</cacheSource></pivotCacheDefinition>)");

  std::stringstream out;
  zip.save(out);
  return std::make_shared<internal::MemoryFile>(out.str());
}

std::string part_of(const Document &document, const std::string &path) {
  std::ostringstream saved;
  document.save(saved);
  const Document reopened =
      open(File::from_memory(saved.str())).as_document_file().document();
  std::ostringstream xml;
  xml << reopened.as_filesystem().open(path).stream()->rdbuf();
  return xml.str();
}

bool contains(const std::string &xml, const std::string &part) {
  return xml.find(part) != std::string::npos;
}

} // namespace

TEST(OoxmlSpreadsheetPivots, a_row_edit_moves_the_source_and_the_place) {
  const Document document = decode(with_pivot());

  first_sheet(document).insert_rows(1, 1);

  EXPECT_TRUE(
      contains(part_of(document, "/xl/pivotCache/pivotCacheDefinition1.xml"),
               R"(<worksheetSource ref="A1:B4" sheet="s"/>)"));
  EXPECT_TRUE(contains(part_of(document, "/xl/pivotTables/pivotTable1.xml"),
                       R"(<location ref="D11:E13")"));
}

TEST(OoxmlSpreadsheetPivots, a_column_edit_moves_them_too) {
  const Document document = decode(with_pivot());

  first_sheet(document).insert_columns(0, 1);

  EXPECT_TRUE(
      contains(part_of(document, "/xl/pivotCache/pivotCacheDefinition1.xml"),
               R"(<worksheetSource ref="B1:C3" sheet="s"/>)"));
  EXPECT_TRUE(contains(part_of(document, "/xl/pivotTables/pivotTable1.xml"),
                       R"(<location ref="E10:F12")"));
}

TEST(OoxmlSpreadsheetPivots,
     an_edit_cutting_the_place_or_taking_the_source_refuses) {
  const Document document = decode(with_pivot());
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.insert_rows(10, 1), UnsupportedOperation);
  EXPECT_THROW(sheet.delete_rows(9, 3), UnsupportedOperation);
  EXPECT_THROW(sheet.delete_rows(0, 3), UnsupportedOperation);
  EXPECT_THROW(sheet.delete_columns(4, 1), UnsupportedOperation);

  sheet.delete_rows(1, 1);
  EXPECT_TRUE(
      contains(part_of(document, "/xl/pivotCache/pivotCacheDefinition1.xml"),
               R"(<worksheetSource ref="A1:B2" sheet="s"/>)"));
}

TEST(OoxmlSpreadsheetPivots, a_source_in_another_workbook_stays) {
  const Document document = decode(
      with_pivot(R"(<worksheetSource ref="A1:B3" sheet="s" r:id="rId1"/>)"));

  first_sheet(document).delete_rows(0, 3);

  EXPECT_TRUE(
      contains(part_of(document, "/xl/pivotCache/pivotCacheDefinition1.xml"),
               R"(<worksheetSource ref="A1:B3" sheet="s" r:id="rId1"/>)"));
}
