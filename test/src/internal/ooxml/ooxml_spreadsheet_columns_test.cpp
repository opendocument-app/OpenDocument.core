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
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

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

/// Row 1 of `s` reads `a`, `b`, `c`.
constexpr const char *abc =
    R"(<row r="1" spans="1:3"><c r="A1" t="inlineStr"><is><t>a</t></is></c>)"
    R"(<c r="B1" t="inlineStr"><is><t>b</t></is></c>)"
    R"(<c r="C1" t="inlineStr"><is><t>c</t></is></c></row>)";

bool contains(const std::string &xml, const std::string &part) {
  return xml.find(part) != std::string::npos;
}

/// An anchor whose corners sit in columns @p from and @p to.
std::string anchor(const std::string &edit_as, const std::uint32_t from,
                   const std::uint32_t to) {
  const auto corner = [](const char *name, const std::uint32_t column) {
    return std::string("<xdr:") + name + "><xdr:col>" + std::to_string(column) +
           "</xdr:col><xdr:colOff>5</xdr:colOff><xdr:row>0</xdr:row>"
           "<xdr:rowOff>0</xdr:rowOff></xdr:" +
           name + ">";
  };
  return R"(<xdr:twoCellAnchor editAs=")" + edit_as + R"(">)" +
         corner("from", from) + corner("to", to) + "</xdr:twoCellAnchor>";
}

std::string note(const std::uint32_t column) {
  return R"(<v:shape><x:ClientData ObjectType="Note"><x:Anchor>)" +
         std::to_string(column) + ", 15, 0, 2, " + std::to_string(column + 2) +
         ", 15, 3, 16</x:Anchor><x:Row>0</x:Row><x:Column>" +
         std::to_string(column) + "</x:Column></x:ClientData></v:shape>";
}

} // namespace

TEST(OoxmlSpreadsheetColumns, an_insert_states_the_cells_past_it_again) {
  const Document document =
      decode(workbook(abc, "", "", "", R"(<dimension ref="A1:C1"/>)"));
  const Sheet sheet = first_sheet(document);

  sheet.insert_columns(1, 2);

  EXPECT_EQ(sheet.cell(0, 0).value().text(), "a");
  EXPECT_EQ(sheet.cell(1, 0).value().type(), ValueType::unknown);
  EXPECT_EQ(sheet.cell(3, 0).value().text(), "b");
  EXPECT_EQ(sheet.cell(4, 0).value().text(), "c");
  EXPECT_EQ(sheet.dimensions().columns, 5);

  const std::string xml = sheet_xml(document);
  EXPECT_TRUE(contains(xml, R"(<c r="E1" t="inlineStr">)"));
  EXPECT_TRUE(contains(xml, R"(<dimension ref="A1:E1"/>)"));
  EXPECT_FALSE(contains(xml, "spans"));
}

TEST(OoxmlSpreadsheetColumns, a_delete_removes_the_cells_and_moves_the_rest) {
  const Document document = decode(workbook(abc));
  const Sheet sheet = first_sheet(document);

  sheet.delete_columns(0, 2);

  EXPECT_EQ(sheet.cell(0, 0).value().text(), "c");
  const std::string xml = sheet_xml(document);
  EXPECT_FALSE(contains(xml, ">a<"));
  EXPECT_TRUE(contains(xml, R"(<c r="A1" t="inlineStr"><is><t>c</t>)"));
}

TEST(OoxmlSpreadsheetColumns, a_formula_moves_with_the_cells_it_reads) {
  const Document document = decode(
      workbook(R"(<row r="1"><c r="A1"><v>1</v></c><c r="B1"><v>2</v></c>)"
               R"(<c r="D1"><f>SUM(A1:C1)+$C$1</f><v>3</v></c></row>)"));

  first_sheet(document).insert_columns(1, 1);

  EXPECT_TRUE(contains(sheet_xml(document),
                       R"(<c r="E1"><f>SUM(A1:D1)+$D$1</f><v>3</v></c>)"));
}

TEST(OoxmlSpreadsheetColumns, a_deleted_reference_becomes_an_error) {
  const Document document = decode(
      workbook(R"(<row r="1"><c r="A1"><v>1</v></c><c r="B1"><v>2</v></c>)"
               R"(<c r="C1"><f>B1*2</f></c></row>)"));

  first_sheet(document).delete_columns(1, 1);

  // the save computes the error
  EXPECT_TRUE(contains(sheet_xml(document),
                       R"(<c r="B1" t="e"><f>#REF!*2</f><v>#REF!</v></c>)"));
}

TEST(OoxmlSpreadsheetColumns, a_shared_group_moves_along_columns) {
  const Document document = decode(workbook(
      R"(<row r="2"><c r="B2"><f t="shared" ref="B2:C2" si="0">A2*2</f></c>)"
      R"(<c r="C2"><f t="shared" si="0"/></c></row>)"));
  const Sheet sheet = first_sheet(document);

  sheet.insert_columns(0, 1);

  EXPECT_TRUE(contains(sheet_xml(document),
                       R"(<f t="shared" ref="C2:D2" si="0">B2*2</f>)"));
  EXPECT_EQ(sheet.cell(3, 1).value().formula(), "C2*2");
}

TEST(OoxmlSpreadsheetColumns, the_column_declarations_move_and_cut) {
  const std::string cols =
      R"(<cols><col min="1" max="3" width="20" customWidth="1"/>)"
      R"(<col min="5" max="5" width="9" customWidth="1"/></cols>)";
  const Document inserted = decode(workbook(abc, "", "", "", cols));
  first_sheet(inserted).insert_columns(1, 1);
  EXPECT_TRUE(contains(sheet_xml(inserted),
                       R"(<col min="1" max="1" width="20" customWidth="1"/>)"
                       R"(<col min="3" max="4" width="20" customWidth="1"/>)"
                       R"(<col min="6" max="6" width="9" customWidth="1"/>)"));

  const Document deleted = decode(workbook(abc, "", "", "", cols));
  first_sheet(deleted).delete_columns(1, 1);
  EXPECT_TRUE(contains(sheet_xml(deleted),
                       R"(<col min="1" max="2" width="20" customWidth="1"/>)"
                       R"(<col min="4" max="4" width="9" customWidth="1"/>)"));
}

TEST(OoxmlSpreadsheetColumns, a_declaration_ends_at_the_last_column) {
  const Document document = decode(
      workbook(abc, "", "", "",
               R"(<cols><col min="1" max="16384" width="20" customWidth="1"/>)"
               R"(</cols>)"));
  first_sheet(document).insert_columns(1, 2);
  EXPECT_TRUE(contains(sheet_xml(document),
                       R"(<col min="1" max="1" width="20" customWidth="1"/>)"
                       R"(<col min="4" max="16384" width="20" )"
                       R"(customWidth="1"/></cols>)"));
}

TEST(OoxmlSpreadsheetColumns, a_merge_moves_and_one_the_edit_cuts_refuses) {
  const Document document = decode(workbook(
      abc, R"(<mergeCells count="1"><mergeCell ref="B1:C1"/></mergeCells>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.insert_columns(2, 1), UnsupportedOperation);
  EXPECT_THROW(sheet.delete_columns(2, 1), UnsupportedOperation);

  sheet.insert_columns(1, 1);
  EXPECT_TRUE(contains(sheet_xml(document), R"(<mergeCell ref="C1:D1"/>)"));
}

TEST(OoxmlSpreadsheetColumns, an_array_formula_the_edit_cuts_refuses) {
  const Document document = decode(workbook(
      R"(<row r="1"><c r="A1"><f t="array" ref="A1:B1">C1:D1*2</f></c></row>)"));

  EXPECT_THROW(first_sheet(document).insert_columns(1, 1),
               UnsupportedOperation);
}

TEST(OoxmlSpreadsheetColumns, an_insert_pushing_a_cell_off_the_grid_refuses) {
  const Document document =
      decode(workbook(R"(<row r="1"><c r="XFD1"><v>1</v></c></row>)"));

  EXPECT_THROW(first_sheet(document).insert_columns(0, 1),
               UnsupportedOperation);
}

TEST(OoxmlSpreadsheetColumns, a_defined_name_moves) {
  const Document document = decode(workbook(
      abc, "", "",
      R"(<definedNames><definedName name="r">s!$B$1:$C$1</definedName>)"
      R"(</definedNames>)"));

  first_sheet(document).insert_columns(0, 1);

  EXPECT_TRUE(contains(part_of(document, "/xl/workbook.xml"),
                       R"(<definedName name="r">s!$C$1:$D$1)"));
}

TEST(OoxmlSpreadsheetColumns, the_ops_name_a_column_and_a_count) {
  const Document document = decode(workbook(abc));

  document.edit(
      R"({"version": 2, "ops": [)"
      R"({"op": "insertColumns", "sheet": 0, "column": 0, "count": 1},)"
      R"({"op": "deleteColumns", "sheet": 0, "column": 2, "count": 1}]})");

  const Sheet sheet = first_sheet(document);
  EXPECT_EQ(sheet.cell(1, 0).value().text(), "a");
  EXPECT_EQ(sheet.cell(2, 0).value().text(), "c");
}

TEST(OoxmlSpreadsheetColumns, the_ranges_of_the_sheet_move) {
  const Document document = decode(workbook(
      abc,
      R"(<autoFilter ref="A1:C1"><filterColumn colId="0"/>)"
      R"(<filterColumn colId="1"/><filterColumn colId="2"/></autoFilter>)"
      R"(<conditionalFormatting sqref="A1:C1 C3"><cfRule/></conditionalFormatting>)"
      R"(<hyperlinks><hyperlink ref="B1"/></hyperlinks>)",
      "", "",
      R"(<sheetViews><sheetView><selection activeCell="B1" sqref="B1"/>)"
      R"(</sheetView></sheetViews>)"));

  first_sheet(document).delete_columns(1, 1);

  const std::string xml = sheet_xml(document);
  EXPECT_TRUE(contains(xml,
                       R"(<autoFilter ref="A1:B1"><filterColumn colId="0"/>)"
                       R"(<filterColumn colId="1"/></autoFilter>)"));
  EXPECT_TRUE(contains(xml, R"(<conditionalFormatting sqref="A1:B1 B3">)"));
  EXPECT_FALSE(contains(xml, "hyperlink"));
  EXPECT_TRUE(contains(xml, R"(<selection activeCell="B1" sqref="B1"/>)"));
}

TEST(OoxmlSpreadsheetColumns, a_drawing_moves_with_its_cells) {
  const Document document = decode(
      workbook_with_parts(abc,
                          anchor("twoCell", 0, 2) + anchor("oneCell", 1, 2) +
                              anchor("absolute", 1, 2),
                          "", ""));

  first_sheet(document).insert_columns(1, 2);

  const std::string xml = part_of(document, "/xl/drawings/drawing1.xml");
  EXPECT_TRUE(contains(xml, anchor("twoCell", 0, 4)));
  EXPECT_TRUE(contains(xml, anchor("oneCell", 3, 4)));
  EXPECT_TRUE(contains(xml, anchor("absolute", 1, 2)));
}

TEST(OoxmlSpreadsheetColumns, a_comment_moves_with_its_note) {
  const Document document = decode(workbook_with_parts(
      abc, "",
      R"(<comment ref="B1" authorId="0"/><comment ref="C1" authorId="0"/>)",
      note(1) + note(2)));

  first_sheet(document).delete_columns(1, 1);

  EXPECT_TRUE(contains(part_of(document, "/xl/comments1.xml"),
                       R"(<commentList><comment ref="B1" authorId="0"/>)"
                       R"(</commentList>)"));
  const std::string notes = part_of(document, "/xl/drawings/vmlDrawing1.vml");
  EXPECT_TRUE(contains(notes, "<x:Column>1</x:Column>"));
  EXPECT_FALSE(contains(notes, "<x:Column>2</x:Column>"));
  EXPECT_TRUE(
      contains(notes, "<x:Anchor>1, 15, 0, 2, 3, 15, 3, 16</x:Anchor>"));
}
