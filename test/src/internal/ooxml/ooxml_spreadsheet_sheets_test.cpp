#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/filesystem.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <iterator>
#include <memory>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

/// Three sheets `s`, `t` and `u` with sheetIds 3, 7 and 9, each holding
/// @p data. @p workbook_extra goes after `<sheets>`, @p t_extra after the
/// `sheetData` of `t`, and @p calc_chain into `calcChain.xml`.
std::shared_ptr<internal::abstract::File>
three_sheets(const std::string &data, const std::string &workbook_extra = "",
             const std::string &calc_chain = "",
             const std::string &t_extra = "") {
  internal::zip::ZipArchive zip;
  insert(
      zip, "[Content_Types].xml",
      R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
      R"(<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>)"
      R"(<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>)"
      R"(<Override PartName="/xl/worksheets/sheet2.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>)"
      R"(<Override PartName="/xl/worksheets/sheet3.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>)"
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
      R"(<bookViews><workbookView activeTab="2"/></bookViews>)"
      R"(<sheets><sheet name="s" sheetId="3" r:id="rId1"/>)"
      R"(<sheet name="t" sheetId="7" r:id="rId2"/>)"
      R"(<sheet name="u" sheetId="9" r:id="rId3"/></sheets>)" +
          workbook_extra + R"(</workbook>)");
  insert(
      zip, "xl/_rels/workbook.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
      R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/>)"
      R"(<Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet3.xml"/>)"
      R"(<Relationship Id="rId4" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/calcChain" Target="calcChain.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/styles.xml",
      R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"/>)");
  for (const char *path :
       {"xl/worksheets/sheet1.xml", "xl/worksheets/sheet2.xml",
        "xl/worksheets/sheet3.xml"}) {
    insert(
        zip, path,
        R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
        R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
        R"(<sheetData>)" +
            data + R"(</sheetData>)" +
            (std::string(path) == "xl/worksheets/sheet2.xml" ? t_extra : "") +
            R"(</worksheet>)");
  }
  insert(
      zip, "xl/worksheets/_rels/sheet2.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships"/>)");
  if (!calc_chain.empty()) {
    insert(
        zip, "xl/calcChain.xml",
        R"(<calcChain xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)" +
            calc_chain + R"(</calcChain>)");
  }

  std::stringstream out;
  zip.save(out);
  return std::make_shared<internal::MemoryFile>(out.str());
}

Sheet sheet_at(const Document &document, const std::uint32_t index) {
  auto it = document.root_element().children().begin();
  std::advance(it, index);
  return (*it).as_sheet();
}

bool contains(const std::string &xml, const std::string &part) {
  return xml.find(part) != std::string::npos;
}

} // namespace

TEST(OoxmlSpreadsheetSheets, a_removed_sheet_leaves_the_package) {
  const Document document = decode(three_sheets(""));

  document.remove(sheet_at(document, 1));

  const Document saved = saved_document(document);
  const Filesystem files = saved.as_filesystem();
  EXPECT_FALSE(files.is_file("/xl/worksheets/sheet2.xml"));
  EXPECT_FALSE(files.is_file("/xl/worksheets/_rels/sheet2.xml.rels"));
  EXPECT_TRUE(files.is_file("/xl/worksheets/sheet3.xml"));
  const std::string workbook = saved_part(document, "/xl/workbook.xml");
  EXPECT_FALSE(contains(workbook, R"(name="t")"));
  EXPECT_FALSE(contains(saved_part(document, "/xl/_rels/workbook.xml.rels"),
                        "sheet2.xml"));
  EXPECT_FALSE(
      contains(saved_part(document, "/[Content_Types].xml"), "sheet2.xml"));
}

TEST(OoxmlSpreadsheetSheets, names_and_views_follow_a_removal) {
  const Document document = decode(three_sheets(
      "", R"(<definedNames>)"
          R"(<definedName name="a" localSheetId="0">$A$1</definedName>)"
          R"(<definedName name="b" localSheetId="1">$A$1</definedName>)"
          R"(<definedName name="c" localSheetId="2">$A$1</definedName>)"
          R"(<definedName name="d">t!$A$1</definedName>)"
          R"(</definedNames>)"));

  document.remove(sheet_at(document, 1));

  const std::string workbook = saved_part(document, "/xl/workbook.xml");
  EXPECT_TRUE(contains(workbook, R"(name="a" localSheetId="0")"));
  EXPECT_FALSE(contains(workbook, R"(name="b")"));
  EXPECT_TRUE(contains(workbook, R"(name="c" localSheetId="1")"));
  EXPECT_TRUE(contains(workbook, R"(<definedName name="d">#REF!<)"));
  EXPECT_TRUE(contains(workbook, R"(activeTab="1")"));
}

TEST(OoxmlSpreadsheetSheets, names_and_views_follow_a_move) {
  const Document document = decode(three_sheets(
      "", R"(<definedNames>)"
          R"(<definedName name="a" localSheetId="0">$A$1</definedName>)"
          R"(<definedName name="c" localSheetId="2">$A$1</definedName>)"
          R"(</definedNames>)"));

  document.move_sheet(sheet_at(document, 2), 0);

  EXPECT_EQ(sheet_at(document, 0).name(), "u");
  const std::string workbook = saved_part(document, "/xl/workbook.xml");
  EXPECT_TRUE(contains(workbook, R"(name="a" localSheetId="1")"));
  EXPECT_TRUE(contains(workbook, R"(name="c" localSheetId="0")"));
  EXPECT_TRUE(contains(workbook, R"(activeTab="0")"));
  EXPECT_LT(workbook.find(R"(name="u")"), workbook.find(R"(name="s")"));
}

TEST(OoxmlSpreadsheetSheets, the_calc_chain_loses_the_removed_sheet) {
  const Document document =
      decode(three_sheets(R"(<row r="1"><c r="A1"><f>1+1</f></c></row>)", "",
                          R"(<c r="A1" i="3"/><c r="A1" i="7"/>)"
                          R"(<c r="A1" i="9"/>)"));

  document.remove(sheet_at(document, 1));

  const std::string chain = saved_part(document, "/xl/calcChain.xml");
  EXPECT_TRUE(contains(chain, R"(i="3")"));
  EXPECT_FALSE(contains(chain, R"(i="7")"));
}

TEST(OoxmlSpreadsheetSheets, refuses_to_remove_a_sheet_holding_a_table) {
  const Document document = decode(three_sheets(
      "", "", "",
      R"(<tableParts count="1"><tablePart r:id="rId1"/></tableParts>)"));

  EXPECT_THROW(document.remove(sheet_at(document, 1)), UnsupportedOperation);
  EXPECT_EQ(sheet_at(document, 1).name(), "t");
}
