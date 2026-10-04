#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/open_strategy.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

/// A flat spreadsheet of the sheets @p tables spell, after @p body.
std::string flat_spreadsheet(const std::string &tables,
                             const std::string &body = "") {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.spreadsheet">)"
         R"(<office:body><office:spreadsheet>)" +
         tables + body +
         R"(</office:spreadsheet></office:body></office:document>)";
}

std::string table(const std::string &name, const std::string &rows) {
  return R"(<table:table table:name=")" + name + R"(">)" + rows +
         R"(</table:table>)";
}

std::string row(const std::string &cells, const std::uint32_t repeated = 1) {
  return R"(<table:table-row)" +
         (repeated > 1 ? R"( table:number-rows-repeated=")" +
                             std::to_string(repeated) + R"(")"
                       : std::string()) +
         ">" + cells + R"(</table:table-row>)";
}

std::string string_cell(const std::string &text) {
  return R"(<table:table-cell office:value-type="string"><text:p>)" + text +
         R"(</text:p></table:table-cell>)";
}

std::string formula_cell(const std::string &formula) {
  return R"(<table:table-cell table:formula=")" + formula +
         R"(" office:value-type="float" office:value="1"><text:p>1</text:p>)"
         R"(</table:table-cell>)";
}

/// A sheet `s` whose first column reads `a`, `b`, `c`.
std::string abc() {
  return table("s", row(string_cell("a")) + row(string_cell("b")) +
                        row(string_cell("c")));
}

Document document_of(const std::string &source) {
  return DecodedFile(
             open_strategy::open_file(std::make_shared<MemoryFile>(source), {},
                                      Logger::null()))
      .as_document_file()
      .document();
}

Sheet sheet_at(const Document &document, const std::uint32_t index) {
  auto it = document.root_element().children().begin();
  for (std::uint32_t i = 0; i < index; ++i) {
    ++it;
  }
  return (*it).as_sheet();
}

std::string text_at(const Sheet &sheet, const std::uint32_t column,
                    const std::uint32_t row) {
  return sheet.cell(column, row).value().text();
}

std::string saved(const Document &document) {
  std::ostringstream out;
  document.save(out);
  return out.str();
}

} // namespace

TEST(OdfSheetRows, an_insert_moves_the_rows_below_down) {
  const Document document = document_of(flat_spreadsheet(abc()));
  const Sheet sheet = sheet_at(document, 0);

  sheet.insert_rows(1, 2);

  EXPECT_EQ(text_at(sheet, 0, 0), "a");
  EXPECT_EQ(sheet.cell(0, 1).value().type(), ValueType::unknown);
  EXPECT_EQ(sheet.cell(0, 2).value().type(), ValueType::unknown);
  EXPECT_EQ(text_at(sheet, 0, 3), "b");
  EXPECT_EQ(text_at(sheet, 0, 4), "c");
  EXPECT_EQ(sheet.dimensions().rows, 5);
}

TEST(OdfSheetRows, an_insert_cuts_the_repeated_run_it_falls_in) {
  const Document document =
      document_of(flat_spreadsheet(table("s", row(string_cell("x"), 3))));
  const Sheet sheet = sheet_at(document, 0);

  sheet.insert_rows(1, 1);

  EXPECT_EQ(text_at(sheet, 0, 0), "x");
  EXPECT_EQ(sheet.cell(0, 1).value().type(), ValueType::unknown);
  EXPECT_EQ(text_at(sheet, 0, 2), "x");
  EXPECT_EQ(text_at(sheet, 0, 3), "x");
  EXPECT_EQ(sheet.dimensions().rows, 4);
}

TEST(OdfSheetRows, a_delete_moves_the_rows_below_up) {
  const Document document = document_of(flat_spreadsheet(abc()));
  const Sheet sheet = sheet_at(document, 0);

  sheet.delete_rows(0, 2);

  EXPECT_EQ(text_at(sheet, 0, 0), "c");
  EXPECT_EQ(sheet.dimensions().rows, 1);
}

TEST(OdfSheetRows, a_delete_cuts_the_repeated_runs_at_both_ends) {
  const Document document = document_of(flat_spreadsheet(
      table("s", row(string_cell("x"), 3) + row(string_cell("y"), 3))));
  const Sheet sheet = sheet_at(document, 0);

  sheet.delete_rows(2, 2);

  EXPECT_EQ(text_at(sheet, 0, 1), "x");
  EXPECT_EQ(text_at(sheet, 0, 2), "y");
  EXPECT_EQ(text_at(sheet, 0, 3), "y");
  EXPECT_EQ(sheet.dimensions().rows, 4);
}

TEST(OdfSheetRows, a_trailing_empty_run_gives_up_what_an_insert_adds) {
  const Document document = document_of(flat_spreadsheet(
      table("s", row(string_cell("a")) + row("<table:table-cell/>", 1000))));
  const Sheet sheet = sheet_at(document, 0);

  sheet.insert_rows(0, 2);

  EXPECT_EQ(text_at(sheet, 0, 2), "a");
  EXPECT_EQ(sheet.dimensions().rows, 1001);
}

TEST(OdfSheetRows, an_insert_past_the_stated_rows_writes_none) {
  const Document document = document_of(flat_spreadsheet(abc()));
  const Sheet sheet = sheet_at(document, 0);

  sheet.insert_rows(10, 2);

  EXPECT_EQ(text_at(sheet, 0, 2), "c");
  EXPECT_EQ(sheet.dimensions().rows, 3);
}

TEST(OdfSheetRows, a_formula_moves_with_the_cells_it_reads) {
  const Document document = document_of(flat_spreadsheet(
      table("s", row(string_cell("a") + formula_cell("of:=[.A3]+[.$A$1]")) +
                     row(string_cell("b")) + row(string_cell("c")))));
  const Sheet sheet = sheet_at(document, 0);

  sheet.insert_rows(1, 1);

  const std::string xml = saved(document);
  EXPECT_NE(xml.find(R"(table:formula="of:=[.A4]+[.$A$1]")"),
            std::string::npos);
  // a reference that only moved still reads the same value
  EXPECT_NE(xml.find(R"(office:value="1")"), std::string::npos);
}

TEST(OdfSheetRows, a_range_an_insert_grows_loses_its_result) {
  const Document document = document_of(flat_spreadsheet(
      table("s", row(string_cell("a") + formula_cell("of:=ROWS([.A1:.A3])")) +
                     row(string_cell("b")) + row(string_cell("c")))));
  const Sheet sheet = sheet_at(document, 0);

  sheet.insert_rows(1, 1);

  const std::string xml = saved(document);
  EXPECT_NE(xml.find(R"x(table:formula="of:=ROWS([.A1:.A4])")x"),
            std::string::npos);
  EXPECT_EQ(xml.find(R"(office:value="1")"), std::string::npos);
  EXPECT_FALSE(sheet.cell(1, 0).value().has_number());
}

TEST(OdfSheetRows, what_reads_a_repeat_of_a_formula_loses_its_result) {
  const Document document = document_of(flat_spreadsheet(
      table("s", row(string_cell("a") +
                     R"(<table:table-cell table:number-columns-repeated="2")"
                     R"x( table:formula="of:=ROWS([.$A$1:.$A$3])")x"
                     R"( office:value-type="float" office:value="3"/>)" +
                     formula_cell("of:=[.C1]")) +
                     row(string_cell("b")) + row(string_cell("c")))));
  const Sheet sheet = sheet_at(document, 0);

  sheet.insert_rows(1, 1);

  EXPECT_FALSE(sheet.cell(3, 0).value().has_number());
}

TEST(OdfSheetRows, a_deleted_reference_becomes_an_error) {
  const Document document = document_of(flat_spreadsheet(
      table("s", row(string_cell("a")) + row(string_cell("b")) +
                     row(string_cell("c") + formula_cell("of:=[.A2]")))));
  const Sheet sheet = sheet_at(document, 0);

  sheet.delete_rows(1, 1);

  const std::string xml = saved(document);
  EXPECT_NE(xml.find(R"(table:formula="of:=#REF!")"), std::string::npos);
  EXPECT_EQ(xml.find(R"(office:value="1")"), std::string::npos);
}

TEST(OdfSheetRows, a_formula_on_another_sheet_moves_where_it_names_the_sheet) {
  const Document document = document_of(flat_spreadsheet(
      abc() + table("t", row(formula_cell("of:=[$s.A2]+[.A2]")))));

  sheet_at(document, 0).insert_rows(0, 1);

  EXPECT_NE(saved(document).find(R"(table:formula="of:=[$s.A3]+[.A2]")"),
            std::string::npos);
}

TEST(OdfSheetRows, a_named_range_moves) {
  const Document document = document_of(flat_spreadsheet(
      abc(), R"(<table:named-expressions><table:named-range table:name="r")"
             R"( table:base-cell-address="$s.$A$1")"
             R"( table:cell-range-address="$s.$A$2:.$A$3"/>)"
             R"(</table:named-expressions>)"));

  sheet_at(document, 0).insert_rows(0, 1);

  const std::string xml = saved(document);
  EXPECT_NE(xml.find(R"(table:cell-range-address="$s.$A$3:.$A$4")"),
            std::string::npos);
  EXPECT_NE(xml.find(R"(table:base-cell-address="$s.$A$2")"),
            std::string::npos);
}

TEST(OdfSheetRows, an_edit_cutting_a_merge_refuses) {
  const Document document = document_of(flat_spreadsheet(table(
      "s", row(R"(<table:table-cell table:number-rows-spanned="2"/>)") +
               row("<table:covered-table-cell/>") + row(string_cell("c")))));
  const Sheet sheet = sheet_at(document, 0);

  EXPECT_THROW(sheet.insert_rows(1, 1), UnsupportedOperation);
  EXPECT_THROW(sheet.delete_rows(1, 1), UnsupportedOperation);
  EXPECT_THROW(sheet.delete_rows(0, 1), UnsupportedOperation);
  EXPECT_EQ(text_at(sheet, 0, 2), "c");

  sheet.insert_rows(2, 1);
  EXPECT_EQ(text_at(sheet, 0, 3), "c");
  sheet.delete_rows(0, 2);
  EXPECT_EQ(text_at(sheet, 0, 1), "c");
}

TEST(OdfSheetRows, an_insert_pushing_a_cell_off_the_grid_refuses) {
  const Document document =
      document_of(flat_spreadsheet(table("s", row(string_cell("x"), 1048576))));

  EXPECT_THROW(sheet_at(document, 0).insert_rows(0, 1), UnsupportedOperation);
}

TEST(OdfSheetRows, the_ops_name_a_row_and_a_count) {
  const Document document = document_of(flat_spreadsheet(abc()));

  document.edit(R"({"version": 2, "ops": [)"
                R"({"op": "insertRows", "sheet": 0, "row": 0, "count": 2},)"
                R"({"op": "deleteRows", "sheet": 0, "row": 3, "count": 1}]})");

  const Sheet sheet = sheet_at(document, 0);
  EXPECT_EQ(text_at(sheet, 0, 2), "a");
  EXPECT_EQ(text_at(sheet, 0, 3), "c");
}

TEST(OdfSheetRows, an_edited_sheet_saves_and_reopens) {
  const Document document = document_of(flat_spreadsheet(abc()));
  sheet_at(document, 0).insert_rows(1, 1);
  sheet_at(document, 0).delete_rows(0, 1);

  const Document reopened = document_of(saved(document));
  const Sheet sheet = sheet_at(reopened, 0);
  EXPECT_EQ(sheet.cell(0, 0).value().type(), ValueType::unknown);
  EXPECT_EQ(text_at(sheet, 0, 1), "b");
  EXPECT_EQ(text_at(sheet, 0, 2), "c");
}

TEST(OdfSheetRows, the_dependents_follow_the_moved_formulas) {
  const Document document = document_of(flat_spreadsheet(
      table("s", row(string_cell("a") + formula_cell("of:=[.A2]")) +
                     row(string_cell("b")))));

  sheet_at(document, 0).insert_rows(0, 1);

  const std::vector<SheetPosition> dependents =
      document.dependents(SheetPosition(0, 0, 2));
  ASSERT_EQ(dependents.size(), 1);
  EXPECT_EQ(dependents[0], SheetPosition(0, 1, 1));
}

TEST(OdfSheetRows, the_references_of_a_condition_move) {
  const Document document = document_of(flat_spreadsheet(
      table(
          "s",
          row(string_cell("a")) + row(string_cell("b")) +
              R"x(<calcext:conditional-formats><calcext:conditional-format)x"
              R"x( calcext:target-range-address="s.A1:s.A2">)x"
              R"x(<calcext:condition calcext:value="formula-is([.A1]&gt;[.$B$1])")x"
              R"x( calcext:base-cell-address="s.A1"/>)x"
              R"x(</calcext:conditional-format></calcext:conditional-formats>)x"),
      R"x(<table:content-validations><table:content-validation table:name="v")x"
      R"x( table:condition="of:cell-content()&lt;[.$C$3] and &quot;[.A1]&quot;&lt;&gt;[.A1]")x"
      R"x( table:base-cell-address="s.A2"/></table:content-validations>)x"));

  sheet_at(document, 0).insert_rows(0, 1);

  const std::string xml = saved(document);
  EXPECT_NE(xml.find(R"x(calcext:value="formula-is([.A2]>[.$B$2])")x"),
            std::string::npos);
  EXPECT_NE(xml.find(R"x(calcext:target-range-address="s.A2:s.A3")x"),
            std::string::npos);
  // the string literal is no reference; pugixml writes `>` plain
  EXPECT_NE(
      xml.find(
          R"x(table:condition="of:cell-content()&lt;[.$C$4] and &quot;[.A1]&quot;&lt;>[.A2]")x"),
      std::string::npos);
  EXPECT_NE(xml.find(R"x(table:base-cell-address="s.A3")x"), std::string::npos);
}

TEST(OdfSheetRows, the_condition_of_a_cell_style_moves) {
  const Document document = document_of(
      R"x(<?xml version="1.0" encoding="UTF-8"?>)x"
      R"x(<office:document office:mimetype=")x"
      R"x(application/vnd.oasis.opendocument.spreadsheet">)x"
      R"x(<office:automatic-styles><style:style style:name="ce1")x"
      R"x( style:family="table-cell"><style:map style:condition="cell-content()&gt;[.B1]")x"
      R"x( style:apply-style-name="Default" style:base-cell-address="s.A1"/>)x"
      R"x(</style:style></office:automatic-styles>)x"
      R"x(<office:body><office:spreadsheet>)x" +
      abc() + R"x(</office:spreadsheet></office:body></office:document>)x");

  sheet_at(document, 0).insert_rows(0, 1);

  const std::string xml = saved(document);
  EXPECT_NE(xml.find(R"x(style:condition="cell-content()>[.B2]")x"),
            std::string::npos);
  EXPECT_NE(xml.find(R"x(style:base-cell-address="s.A2")x"), std::string::npos);
}

TEST(OdfSheetRows, a_condition_reads_from_its_first_cell_that_stays) {
  const Document document = document_of(flat_spreadsheet(table(
      "s",
      row(string_cell("a")) + row(string_cell("b")) + row(string_cell("c")) +
          R"x(<calcext:conditional-formats><calcext:conditional-format)x"
          R"x( calcext:target-range-address="s.A1:s.A3">)x"
          R"x(<calcext:condition calcext:value="formula-is([.A1]&gt;[.$B$1])")x"
          R"x( calcext:base-cell-address="s.A1"/>)x"
          R"x(</calcext:conditional-format></calcext:conditional-formats>)x")));

  sheet_at(document, 0).delete_rows(0, 1);

  const std::string xml = saved(document);
  EXPECT_NE(xml.find(R"x(calcext:value="formula-is([.A1]>#REF!)")x"),
            std::string::npos);
  EXPECT_NE(xml.find(R"x(calcext:target-range-address="s.A1:s.A2")x"),
            std::string::npos);
  EXPECT_NE(xml.find(R"x(calcext:base-cell-address="s.A1")x"),
            std::string::npos);
}
