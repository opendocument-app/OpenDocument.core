#include <odr/document.hpp>
#include <odr/document_element.hpp>
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

/// Sheet `s` with a chart drawing `A1:A3` against `B1:B3`, titled by `C1`.
std::shared_ptr<internal::abstract::File> with_chart() {
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
      R"(<sheets><sheet name="s" sheetId="1" r:id="rId1"/></sheets></workbook>)");
  insert(
      zip, "xl/_rels/workbook.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/styles.xml",
      R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"/>)");
  insert(
      zip, "xl/worksheets/sheet1.xml",
      R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
      R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
      R"(<sheetData><row r="1"><c r="A1"><v>1</v></c></row></sheetData>)"
      R"(<drawing r:id="rId1"/></worksheet>)");
  insert(
      zip, "xl/worksheets/_rels/sheet1.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/drawing" Target="../drawings/drawing1.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/drawings/drawing1.xml",
      R"(<xdr:wsDr xmlns:xdr="http://schemas.openxmlformats.org/drawingml/2006/spreadsheetDrawing" )"
      R"(xmlns:a="http://schemas.openxmlformats.org/drawingml/2006/main" )"
      R"(xmlns:c="http://schemas.openxmlformats.org/drawingml/2006/chart" )"
      R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
      R"(<xdr:twoCellAnchor><xdr:from><xdr:col>4</xdr:col><xdr:colOff>0</xdr:colOff>)"
      R"(<xdr:row>0</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:from>)"
      R"(<xdr:to><xdr:col>8</xdr:col><xdr:colOff>0</xdr:colOff>)"
      R"(<xdr:row>9</xdr:row><xdr:rowOff>0</xdr:rowOff></xdr:to>)"
      R"(<xdr:graphicFrame><a:graphic><a:graphicData><c:chart r:id="rId1"/>)"
      R"(</a:graphicData></a:graphic></xdr:graphicFrame><xdr:clientData/>)"
      R"(</xdr:twoCellAnchor></xdr:wsDr>)");
  insert(
      zip, "xl/drawings/_rels/drawing1.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/chart" Target="../charts/chart1.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/charts/chart1.xml",
      R"(<c:chartSpace xmlns:c="http://schemas.openxmlformats.org/drawingml/2006/chart">)"
      R"(<c:chart><c:plotArea><c:barChart><c:ser>)"
      R"(<c:tx><c:strRef><c:f>s!$C$1</c:f></c:strRef></c:tx>)"
      R"(<c:cat><c:numRef><c:f>s!$A$1:$A$3</c:f><c:numCache><c:ptCount val="3"/></c:numCache></c:numRef></c:cat>)"
      R"(<c:val><c:numRef><c:f>'s'!$B$1:$B$3</c:f></c:numRef></c:val>)"
      R"(</c:ser></c:barChart></c:plotArea></c:chart></c:chartSpace>)");

  std::stringstream out;
  zip.save(out);
  return std::make_shared<internal::MemoryFile>(out.str());
}

std::string chart_xml(const Document &document) {
  std::ostringstream saved;
  document.save(saved);
  const Document reopened =
      open(File::from_memory(saved.str())).as_document_file().document();
  std::ostringstream xml;
  xml << reopened.as_filesystem()
             .open("/xl/charts/chart1.xml")
             .stream()
             ->rdbuf();
  return xml.str();
}

} // namespace

TEST(OoxmlSpreadsheetCharts, a_row_edit_moves_the_ranges_a_chart_reads) {
  const Document document = decode(with_chart());

  first_sheet(document).insert_rows(1, 1);

  const std::string xml = chart_xml(document);
  EXPECT_NE(xml.find("<c:f>s!$A$1:$A$4</c:f>"), std::string::npos);
  EXPECT_NE(xml.find("<c:f>s!$B$1:$B$4</c:f>"), std::string::npos);
  EXPECT_NE(xml.find("<c:f>s!$C$1</c:f>"), std::string::npos);
  EXPECT_NE(xml.find(R"(<c:ptCount val="3"/>)"), std::string::npos);
}

TEST(OoxmlSpreadsheetCharts, a_column_edit_moves_them_and_loses_a_removed_one) {
  const Document document = decode(with_chart());

  first_sheet(document).delete_columns(2, 1);

  const std::string xml = chart_xml(document);
  EXPECT_NE(xml.find("<c:f>#REF!</c:f>"), std::string::npos);
  EXPECT_NE(xml.find("<c:f>s!$A$1:$A$3</c:f>"), std::string::npos);
}
