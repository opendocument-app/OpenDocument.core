#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>
#include <odr/table_dimension.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

/// Two sheets, `s` and `t`, with @p calc_chain as `calcChain.xml` and
/// @p defined_names after the sheets of `workbook.xml`.
std::shared_ptr<internal::abstract::File>
two_sheets(const std::string &s_data, const std::string &t_data,
           const std::string &calc_chain = "",
           const std::string &defined_names = "") {
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
      R"(<sheets><sheet name="s" sheetId="3" r:id="rId1"/>)"
      R"(<sheet name="t" sheetId="7" r:id="rId2"/></sheets>)" +
          defined_names + R"(</workbook>)");
  insert(
      zip, "xl/_rels/workbook.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
      R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet2.xml"/>)"
      R"(<Relationship Id="rId3" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/calcChain" Target="calcChain.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/styles.xml",
      R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"/>)");
  for (const auto &[path, data] :
       {std::pair("xl/worksheets/sheet1.xml", s_data),
        std::pair("xl/worksheets/sheet2.xml", t_data)}) {
    insert(
        zip, path,
        R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)"
        R"(<sheetData>)" +
            data + R"(</sheetData></worksheet>)");
  }
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

/// An anchor whose corners sit in rows @p from and @p to.
std::string anchor(const std::string &edit_as, const std::uint32_t from,
                   const std::uint32_t to) {
  const auto corner = [](const char *name, const std::uint32_t row) {
    return std::string("<xdr:") + name + "><xdr:col>0</xdr:col>" +
           "<xdr:colOff>0</xdr:colOff><xdr:row>" + std::to_string(row) +
           "</xdr:row><xdr:rowOff>5</xdr:rowOff></xdr:" + name + ">";
  };
  return R"(<xdr:twoCellAnchor editAs=")" + edit_as + R"(">)" +
         corner("from", from) + corner("to", to) + "</xdr:twoCellAnchor>";
}

std::string note(const std::uint32_t row) {
  return R"(<v:shape><x:ClientData ObjectType="Note"><x:Anchor>1, 15, )" +
         std::to_string(row) + ", 2, 3, 15, " + std::to_string(row + 3) +
         ", 16</x:Anchor><x:Row>" + std::to_string(row) +
         "</x:Row><x:Column>0</x:Column></x:ClientData></v:shape>";
}

/// A part of the package @p document saves.
std::string part_of(const Document &document, const std::string &path) {
  std::ostringstream saved;
  document.save(saved);
  const Document reopened =
      open(File::from_memory(saved.str())).as_document_file().document();
  std::ostringstream xml;
  xml << reopened.as_filesystem().open(path).stream()->rdbuf();
  return xml.str();
}

std::string sheet_xml(const Document &document) {
  return part_of(document, "/xl/worksheets/sheet1.xml");
}

/// Column A of `s` reads `a`, `b`, `c`.
constexpr const char *abc =
    R"(<row r="1"><c r="A1" t="inlineStr"><is><t>a</t></is></c></row>)"
    R"(<row r="2"><c r="A2" t="inlineStr"><is><t>b</t></is></c></row>)"
    R"(<row r="3"><c r="A3" t="inlineStr"><is><t>c</t></is></c></row>)";

bool contains(const std::string &xml, const std::string &part) {
  return xml.find(part) != std::string::npos;
}

} // namespace

TEST(OoxmlSpreadsheetRows, an_insert_numbers_the_rows_below_again) {
  const Document document =
      decode(workbook(abc, "", "", "", R"(<dimension ref="A1:A3"/>)"));
  const Sheet sheet = first_sheet(document);

  sheet.insert_rows(1, 2);

  EXPECT_EQ(sheet.cell(0, 0).value().text(), "a");
  EXPECT_EQ(sheet.cell(0, 1).value().type(), ValueType::unknown);
  EXPECT_EQ(sheet.cell(0, 3).value().text(), "b");
  EXPECT_EQ(sheet.cell(0, 4).value().text(), "c");
  EXPECT_EQ(sheet.dimensions().rows, 5);

  const std::string xml = sheet_xml(document);
  EXPECT_TRUE(contains(xml, R"(<row r="4"><c r="A4")"));
  EXPECT_TRUE(contains(xml, R"(<dimension ref="A1:A5"/>)"));
}

TEST(OoxmlSpreadsheetRows, a_delete_removes_the_rows_and_numbers_the_rest) {
  const Document document = decode(workbook(abc));
  const Sheet sheet = first_sheet(document);

  sheet.delete_rows(0, 2);

  EXPECT_EQ(sheet.cell(0, 0).value().text(), "c");
  EXPECT_EQ(sheet.cell(0, 1).value().type(), ValueType::unknown);
  const std::string xml = sheet_xml(document);
  EXPECT_FALSE(contains(xml, ">a<"));
  EXPECT_TRUE(contains(xml, R"(<row r="1"><c r="A1")"));
}

TEST(OoxmlSpreadsheetRows, a_formula_moves_with_the_cells_it_reads) {
  const Document document = decode(workbook(
      std::string(abc) +
      R"(<row r="4"><c r="B4"><f>SUM(A1:A3)+$A$3</f><v>1</v></c></row>)"));

  first_sheet(document).insert_rows(1, 1);

  EXPECT_TRUE(contains(sheet_xml(document),
                       R"(<c r="B5"><f>SUM(A1:A4)+$A$4</f><v>1</v></c>)"));
}

TEST(OoxmlSpreadsheetRows, a_deleted_reference_becomes_an_error) {
  const Document document = decode(workbook(
      std::string(abc) + R"(<row r="4"><c r="B4"><f>A2*2</f></c></row>)"));

  first_sheet(document).delete_rows(1, 1);

  EXPECT_TRUE(contains(sheet_xml(document), R"(<c r="B3"><f>#REF!*2</f>)"));
}

TEST(OoxmlSpreadsheetRows, a_formula_on_another_sheet_moves_where_it_names_it) {
  const Document document = decode(
      two_sheets(abc, R"(<row r="1"><c r="A1"><f>s!A2+A2</f></c></row>)"));

  first_sheet(document).insert_rows(0, 1);

  EXPECT_TRUE(contains(part_of(document, "/xl/worksheets/sheet2.xml"),
                       "<f>s!A3+A2</f>"));
}

TEST(OoxmlSpreadsheetRows, a_shared_group_that_still_reads_true_stays_shared) {
  const Document document = decode(workbook(
      R"(<row r="2"><c r="B2"><f t="shared" ref="B2:B3" si="0">A2*2</f></c></row>)"
      R"(<row r="3"><c r="B3"><f t="shared" si="0"/></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.insert_rows(0, 1);

  EXPECT_TRUE(contains(sheet_xml(document),
                       R"(<f t="shared" ref="B3:B4" si="0">A3*2</f>)"));
  EXPECT_EQ(sheet.cell(1, 3).value().formula(), "A4*2");
}

/// The master above the edge reads a cell below it, so after the move its
/// expression shifted to a member no longer says what that member reads.
TEST(OoxmlSpreadsheetRows, a_shared_group_the_move_breaks_is_written_out) {
  const Document document = decode(workbook(
      R"(<row r="2"><c r="B2"><f t="shared" ref="B2:B3" si="0">A5*2</f></c></row>)"
      R"(<row r="3"><c r="B3"><f t="shared" si="0"/></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.insert_rows(2, 1);

  const std::string xml = sheet_xml(document);
  EXPECT_TRUE(contains(xml, R"(<c r="B2"><f>A6*2</f></c>)"));
  EXPECT_TRUE(contains(xml, R"(<c r="B4"><f>A7*2</f></c>)"));
  EXPECT_EQ(sheet.cell(1, 3).value().formula(), "A7*2");
}

TEST(OoxmlSpreadsheetRows, a_shared_group_losing_its_master_is_written_out) {
  const Document document = decode(workbook(
      R"(<row r="2"><c r="B2"><f t="shared" ref="B2:B3" si="0">A2*2</f></c></row>)"
      R"(<row r="3"><c r="B3"><f t="shared" si="0"/></c></row>)"));

  first_sheet(document).delete_rows(1, 1);

  EXPECT_TRUE(contains(sheet_xml(document), R"(<c r="B2"><f>A2*2</f></c>)"));
}

TEST(OoxmlSpreadsheetRows, a_merge_moves_and_one_the_edit_cuts_refuses) {
  const Document document = decode(workbook(
      abc, R"(<mergeCells count="1"><mergeCell ref="B2:C3"/></mergeCells>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.insert_rows(2, 1), UnsupportedOperation);
  EXPECT_THROW(sheet.delete_rows(2, 1), UnsupportedOperation);

  sheet.insert_rows(1, 1);
  EXPECT_TRUE(contains(sheet_xml(document), R"(<mergeCell ref="B3:C4"/>)"));
  sheet.delete_rows(2, 2);
  EXPECT_FALSE(contains(sheet_xml(document), "mergeCell"));
}

TEST(OoxmlSpreadsheetRows, an_array_formula_the_edit_cuts_refuses) {
  const Document document = decode(workbook(
      R"(<row r="1"><c r="B1"><f t="array" ref="B1:B2">A1:A2*2</f></c></row>)"));

  EXPECT_THROW(first_sheet(document).insert_rows(1, 1), UnsupportedOperation);
}

TEST(OoxmlSpreadsheetRows, an_insert_pushing_a_row_off_the_grid_refuses) {
  const Document document = decode(
      workbook(R"(<row r="1048576"><c r="A1048576"><v>1</v></c></row>)"));

  EXPECT_THROW(first_sheet(document).insert_rows(0, 1), UnsupportedOperation);
}

TEST(OoxmlSpreadsheetRows, a_defined_name_moves) {
  const Document document = decode(two_sheets(
      abc, "", "",
      R"(<definedNames><definedName name="r">s!$A$1:$A$3</definedName>)"
      R"(<definedName name="l" localSheetId="0">$A$2</definedName>)"
      R"(</definedNames>)"));

  first_sheet(document).insert_rows(1, 1);

  const std::string xml = part_of(document, "/xl/workbook.xml");
  EXPECT_TRUE(contains(xml, R"(<definedName name="r">s!$A$1:$A$4)"));
  EXPECT_TRUE(contains(xml, R"(localSheetId="0">$A$3<)"));
}

TEST(OoxmlSpreadsheetRows, the_calc_chain_moves_with_the_cells) {
  const Document document = decode(two_sheets(
      std::string(abc) + R"(<row r="4"><c r="B4"><f>A1</f></c></row>)"
                         R"(<row r="5"><c r="B5"><f>A1</f></c></row>)",
      R"(<row r="4"><c r="A4"><f>1</f></c></row>)",
      R"(<c r="B4" i="3"/><c r="B5"/><c r="A4" i="7"/>)"));

  first_sheet(document).delete_rows(3, 1);

  EXPECT_TRUE(contains(part_of(document, "/xl/calcChain.xml"),
                       R"(<c i="3" r="B4"/><c r="A4" i="7"/>)"));
}

TEST(OoxmlSpreadsheetRows, the_ops_name_a_row_and_a_count) {
  const Document document = decode(workbook(abc));

  document.edit(R"({"version": 2, "ops": [)"
                R"({"op": "insertRows", "sheet": 0, "row": 0, "count": 1},)"
                R"({"op": "deleteRows", "sheet": 0, "row": 2, "count": 1}]})");

  const Sheet sheet = first_sheet(document);
  EXPECT_EQ(sheet.cell(0, 1).value().text(), "a");
  EXPECT_EQ(sheet.cell(0, 2).value().text(), "c");
}

TEST(OoxmlSpreadsheetRows, the_ranges_of_the_sheet_move) {
  const Document document = decode(workbook(
      abc,
      R"(<conditionalFormatting sqref="A1:A3 C2"><cfRule/></conditionalFormatting>)"
      R"(<dataValidations count="2"><dataValidation sqref="A2"/>)"
      R"(<dataValidation sqref="A3"/></dataValidations>)"
      R"(<hyperlinks><hyperlink ref="A2"/></hyperlinks>)",
      "", "",
      R"(<sheetViews><sheetView><selection activeCell="A2" sqref="A2"/>)"
      R"(</sheetView></sheetViews>)"));

  first_sheet(document).delete_rows(1, 1);

  const std::string xml = sheet_xml(document);
  EXPECT_TRUE(contains(xml, R"(<conditionalFormatting sqref="A1:A2">)"));
  EXPECT_TRUE(
      contains(xml, R"(<dataValidations count="1"><dataValidation sqref="A2"/>)"
                    R"(</dataValidations>)"));
  EXPECT_FALSE(contains(xml, "hyperlink"));
  EXPECT_TRUE(contains(xml, R"(<selection activeCell="A2" sqref="A2"/>)"));
}

TEST(OoxmlSpreadsheetRows, a_selection_a_delete_takes_falls_back_to_a1) {
  const Document document =
      decode(workbook(abc, "", "", "",
                      R"(<sheetViews><sheetView><selection sqref="A2"/>)"
                      R"(</sheetView></sheetViews>)"));

  first_sheet(document).delete_rows(1, 1);

  EXPECT_TRUE(contains(sheet_xml(document), "<selection/>"));
}

TEST(OoxmlSpreadsheetRows, a_drawing_moves_with_its_cells) {
  const Document document = decode(
      workbook_with_parts(abc,
                          anchor("twoCell", 0, 2) + anchor("oneCell", 1, 2) +
                              anchor("absolute", 1, 2),
                          "", ""));

  first_sheet(document).insert_rows(1, 2);

  const std::string xml = part_of(document, "/xl/drawings/drawing1.xml");
  // the first stretches over the insert, the second moves whole
  EXPECT_TRUE(contains(xml, anchor("twoCell", 0, 4)));
  EXPECT_TRUE(contains(xml, anchor("oneCell", 3, 4)));
  EXPECT_TRUE(contains(xml, anchor("absolute", 1, 2)));
}

TEST(OoxmlSpreadsheetRows, a_one_cell_box_keeps_its_size_over_an_insert) {
  const Document document =
      decode(workbook_with_parts(abc, anchor("oneCell", 0, 2), "", ""));

  first_sheet(document).insert_rows(1, 2);

  EXPECT_TRUE(contains(part_of(document, "/xl/drawings/drawing1.xml"),
                       anchor("oneCell", 0, 2)));
}

TEST(OoxmlSpreadsheetRows, a_comment_moves_with_its_note) {
  const Document document = decode(workbook_with_parts(
      abc, "",
      R"(<comment ref="A2" authorId="0"/><comment ref="A3" authorId="0"/>)",
      note(1) + note(2)));

  first_sheet(document).delete_rows(1, 1);

  EXPECT_EQ(part_of(document, "/xl/comments1.xml")
                    .find(R"(<commentList><comment ref="A2" authorId="0"/>)"
                          R"(</commentList>)") != std::string::npos,
            true);
  const std::string notes = part_of(document, "/xl/drawings/vmlDrawing1.vml");
  EXPECT_TRUE(contains(notes, "<x:Row>1</x:Row>"));
  EXPECT_FALSE(contains(notes, "<x:Row>2</x:Row>"));
  EXPECT_TRUE(
      contains(notes, "<x:Anchor>1, 15, 1, 2, 3, 15, 4, 16</x:Anchor>"));
}

TEST(OoxmlSpreadsheetRows, a_rule_and_a_validation_move_their_formulas) {
  const Document document = decode(workbook(
      abc, R"(<conditionalFormatting sqref="A1:A3"><cfRule type="expression">)"
           R"(<formula>$B$1&lt;A1</formula></cfRule></conditionalFormatting>)"
           R"(<dataValidations count="1"><dataValidation sqref="A2">)"
           R"(<formula1>$C$5</formula1><formula2>A1</formula2>)"
           R"(</dataValidation></dataValidations>)"));

  first_sheet(document).insert_rows(0, 1);

  const std::string xml = sheet_xml(document);
  EXPECT_TRUE(contains(xml, "<formula>$B$2&lt;A2</formula>"));
  EXPECT_TRUE(
      contains(xml, "<formula1>$C$6</formula1><formula2>A2</formula2>"));
}

TEST(OoxmlSpreadsheetRows, a_page_break_moves_and_two_at_one_place_become_one) {
  const std::string breaks =
      R"(<rowBreaks count="2" manualBreakCount="2"><brk id="2" man="1"/>)"
      R"(<brk id="5" man="1"/></rowBreaks>)";
  const Document moved = decode(workbook(abc, breaks));
  first_sheet(moved).delete_rows(2, 2);
  EXPECT_TRUE(contains(sheet_xml(moved),
                       R"(<brk id="2" man="1"/><brk id="3" man="1"/>)"));

  const Document merged = decode(workbook(abc, breaks));
  first_sheet(merged).delete_rows(2, 4);
  EXPECT_TRUE(contains(sheet_xml(merged),
                       R"(<rowBreaks count="1" manualBreakCount="1">)"
                       R"(<brk id="2" man="1"/></rowBreaks>)"));
}

TEST(OoxmlSpreadsheetRows, a_rule_reads_from_its_first_cell_that_stays) {
  const Document document = decode(workbook(
      abc,
      R"(<conditionalFormatting sqref="A1:A3"><cfRule type="expression">)"
      R"(<formula>$B$1&lt;A1</formula></cfRule></conditionalFormatting>)"));

  first_sheet(document).delete_rows(0, 1);

  const std::string xml = sheet_xml(document);
  EXPECT_TRUE(contains(xml, R"(<conditionalFormatting sqref="A1:A2">)"));
  EXPECT_TRUE(contains(xml, "<formula>#REF!&lt;A1</formula>"));
}
