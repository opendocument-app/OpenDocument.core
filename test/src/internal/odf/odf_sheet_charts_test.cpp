#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

void insert(zip::ZipArchive &zip, const std::string &path,
            const std::string &content) {
  zip.insert_file(std::end(zip), RelPath(path),
                  std::make_shared<MemoryFile>(content));
}

/// An ods whose sheet `Výplaty` holds a chart, in a part of its own, reading
/// `A1:A3` and labelled by `B1`, and an embedded spreadsheet with a sheet of
/// the same name.
std::string with_chart() {
  zip::ZipArchive zip;
  insert(zip, "mimetype", "application/vnd.oasis.opendocument.spreadsheet");
  insert(
      zip, "META-INF/manifest.xml",
      R"(<manifest:manifest xmlns:manifest="urn:oasis:names:tc:opendocument:xmlns:manifest:1.0">)"
      R"(<manifest:file-entry manifest:full-path="/" manifest:media-type="application/vnd.oasis.opendocument.spreadsheet"/>)"
      R"(<manifest:file-entry manifest:full-path="content.xml" manifest:media-type="text/xml"/>)"
      R"(</manifest:manifest>)");
  insert(
      zip, "content.xml",
      R"(<office:document-content xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0" )"
      R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" )"
      R"(xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0" )"
      R"(xmlns:xlink="http://www.w3.org/1999/xlink">)"
      R"(<office:body><office:spreadsheet><table:table table:name="Výplaty">)"
      R"(<table:table-row><table:table-cell office:value-type="float" office:value="1">)"
      R"(<draw:frame><draw:object draw:notify-on-update-of-ranges="Výplaty.A1:Výplaty.A3")"
      R"( xlink:href="./Object 1"/></draw:frame>)"
      R"(<draw:frame><draw:object xlink:href="./Object 2"/></draw:frame>)"
      R"(<text:p>1</text:p>)"
      R"(</table:table-cell></table:table-row></table:table>)"
      R"(</office:spreadsheet></office:body></office:document-content>)");
  insert(
      zip, "styles.xml",
      R"(<office:document-styles><office:styles><style:style style:name="ce1")"
      R"( style:family="table-cell"><style:map style:condition="cell-content()&gt;[.B3]")"
      R"( style:apply-style-name="Default" style:base-cell-address="Výplaty.A3"/>)"
      R"(</style:style></office:styles></office:document-styles>)");
  insert(
      zip, "Object 1/content.xml",
      R"(<office:document-content xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0" )"
      R"(xmlns:chart="urn:oasis:names:tc:opendocument:xmlns:chart:1.0">)"
      R"(<office:body><office:chart><chart:chart><chart:plot-area)"
      R"( table:cell-range-address="Výplaty.A1:Výplaty.A3">)"
      R"(<chart:series chart:values-cell-range-address="Výplaty.A1:Výplaty.A3")"
      R"( chart:label-cell-address="Výplaty.B1"/>)"
      R"(<style:chart-properties chart:error-upper-range="Výplaty.C1:Výplaty.C3"/>)"
      R"(</chart:plot-area></chart:chart></office:chart></office:body>)"
      R"(</office:document-content>)");
  insert(
      zip, "Object 2/content.xml",
      R"(<office:document-content xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:table="urn:oasis:names:tc:opendocument:xmlns:table:1.0">)"
      R"(<office:body><office:spreadsheet><table:table table:name="Výplaty"/>)"
      R"(<table:named-expressions><table:named-range table:name="n")"
      R"( table:cell-range-address="Výplaty.A1:Výplaty.A3"/>)"
      R"(</table:named-expressions></office:spreadsheet></office:body>)"
      R"(</office:document-content>)");
  std::stringstream out;
  zip.save(out);
  return out.str();
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

Sheet first_sheet(const Document &document) {
  return (*document.root_element().children().begin()).as_sheet();
}

} // namespace

// a moved address quotes a sheet name past ascii, which ODF allows
TEST(OdfSheetCharts, a_row_edit_moves_the_ranges_a_chart_reads) {
  const Document document =
      open(File::from_memory(with_chart())).as_document_file().document();

  first_sheet(document).insert_rows(1, 1);

  const std::string styles = part_of(document, "/styles.xml");
  EXPECT_NE(styles.find("cell-content()>[.B4]"), std::string::npos);
  EXPECT_NE(styles.find("'Výplaty'.A4"), std::string::npos);
  const std::string chart = part_of(document, "/Object 1/content.xml");
  EXPECT_NE(
      chart.find(R"(table:cell-range-address="'Výplaty'.A1:'Výplaty'.A4")"),
      std::string::npos);
  EXPECT_NE(
      chart.find(
          R"(chart:values-cell-range-address="'Výplaty'.A1:'Výplaty'.A4")"),
      std::string::npos);
  EXPECT_NE(chart.find(R"(chart:label-cell-address="Výplaty.B1")"),
            std::string::npos);
  EXPECT_NE(
      part_of(document, "/content.xml")
          .find(
              R"(draw:notify-on-update-of-ranges="'Výplaty'.A1:'Výplaty'.A4")"),
      std::string::npos);
}

TEST(OdfSheetCharts, an_embedded_spreadsheet_stays) {
  const Document document =
      open(File::from_memory(with_chart())).as_document_file().document();

  first_sheet(document).insert_rows(1, 1);

  EXPECT_NE(part_of(document, "/Object 2/content.xml")
                .find(R"(table:cell-range-address="Výplaty.A1:Výplaty.A3")"),
            std::string::npos);
}

TEST(OdfSheetCharts, a_column_edit_moves_them_too) {
  const Document document =
      open(File::from_memory(with_chart())).as_document_file().document();

  first_sheet(document).insert_columns(0, 1);

  const std::string styles = part_of(document, "/styles.xml");
  EXPECT_NE(styles.find("cell-content()>[.C3]"), std::string::npos);
  EXPECT_NE(styles.find("'Výplaty'.B3"), std::string::npos);
  const std::string chart = part_of(document, "/Object 1/content.xml");
  EXPECT_NE(
      chart.find(
          R"(chart:values-cell-range-address="'Výplaty'.B1:'Výplaty'.B3")"),
      std::string::npos);
  EXPECT_NE(chart.find(R"(chart:label-cell-address="'Výplaty'.C1")"),
            std::string::npos);
  EXPECT_NE(
      chart.find(R"(chart:error-upper-range="'Výplaty'.D1:'Výplaty'.D3")"),
      std::string::npos);
}
