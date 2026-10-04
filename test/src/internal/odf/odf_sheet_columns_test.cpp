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

/// A sheet `s` whose first row reads `a`, `b`, `c`.
std::string abc() {
  return table("s",
               row(string_cell("a") + string_cell("b") + string_cell("c")));
}

std::string empty_cells(const std::uint32_t repeated) {
  return R"(<table:table-cell table:number-columns-repeated=")" +
         std::to_string(repeated) + R"("/>)";
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

TEST(OdfSheetColumns, an_insert_moves_the_cells_right) {
  const Document document = document_of(flat_spreadsheet(abc()));
  const Sheet sheet = sheet_at(document, 0);

  sheet.insert_columns(1, 2);

  EXPECT_EQ(text_at(sheet, 0, 0), "a");
  EXPECT_EQ(sheet.cell(1, 0).value().type(), ValueType::unknown);
  EXPECT_EQ(sheet.cell(2, 0).value().type(), ValueType::unknown);
  EXPECT_EQ(text_at(sheet, 3, 0), "b");
  EXPECT_EQ(text_at(sheet, 4, 0), "c");
}

TEST(OdfSheetColumns, an_insert_cuts_a_repeated_cell_and_a_repeated_row) {
  const Document document = document_of(flat_spreadsheet(table(
      "s",
      row(R"(<table:table-cell table:number-columns-repeated="3")"
          R"( office:value-type="string"><text:p>x</text:p></table:table-cell>)",
          2))));
  const Sheet sheet = sheet_at(document, 0);

  sheet.insert_columns(1, 1);

  for (const std::uint32_t r : {0, 1}) {
    EXPECT_EQ(text_at(sheet, 0, r), "x");
    EXPECT_EQ(sheet.cell(1, r).value().type(), ValueType::unknown);
    EXPECT_EQ(text_at(sheet, 2, r), "x");
    EXPECT_EQ(text_at(sheet, 3, r), "x");
  }
}

TEST(OdfSheetColumns, a_delete_moves_the_cells_left) {
  const Document document = document_of(flat_spreadsheet(abc()));
  const Sheet sheet = sheet_at(document, 0);

  sheet.delete_columns(0, 2);

  EXPECT_EQ(text_at(sheet, 0, 0), "c");
  EXPECT_EQ(sheet.cell(1, 0).value().type(), ValueType::unknown);
}

TEST(OdfSheetColumns, a_trailing_empty_run_gives_up_what_an_insert_adds) {
  const Document document = document_of(flat_spreadsheet(table(
      "s", R"(<table:table-column table:number-columns-repeated="1001"/>)" +
               row(string_cell("a") + empty_cells(1000)))));
  const Sheet sheet = sheet_at(document, 0);

  sheet.insert_columns(0, 2);

  EXPECT_EQ(text_at(sheet, 2, 0), "a");
  EXPECT_EQ(sheet.dimensions().columns, 1001);
  EXPECT_EQ(saved(document).find(R"(table:number-columns-repeated="1000")"),
            std::string::npos);
}

TEST(OdfSheetColumns, a_formula_moves_with_the_cells_it_reads) {
  const Document document = document_of(flat_spreadsheet(
      table("s", row(string_cell("a") + formula_cell("of:=[.C1]+[.$A$1]") +
                     string_cell("c")))));

  const Sheet sheet = sheet_at(document, 0);
  sheet.insert_columns(1, 1);

  // a reference that only moved still reads the same value
  EXPECT_EQ(sheet.cell(2, 0).value().number(), 1);
  // the save computes the result again: `c` and `a` add up to an error
  const std::string xml = saved(document);
  EXPECT_NE(xml.find(R"(table:formula="of:=[.D1]+[.$A$1]")"),
            std::string::npos);
  EXPECT_NE(xml.find(R"(calcext:value-type="error")"), std::string::npos);
}

TEST(OdfSheetColumns, a_deleted_reference_becomes_an_error) {
  const Document document = document_of(
      flat_spreadsheet(table("s", row(string_cell("a") + string_cell("b") +
                                      formula_cell("of:=[.B1]")))));

  sheet_at(document, 0).delete_columns(1, 1);

  const std::string xml = saved(document);
  EXPECT_NE(xml.find(R"(table:formula="of:=#REF!")"), std::string::npos);
  EXPECT_EQ(xml.find(R"(office:value="1")"), std::string::npos);
}

TEST(OdfSheetColumns, a_formula_on_another_sheet_moves_where_it_names_it) {
  const Document document = document_of(flat_spreadsheet(
      abc() + table("t", row(formula_cell("of:=[$s.B1]+[.B1]")))));

  sheet_at(document, 0).insert_columns(0, 1);

  EXPECT_NE(saved(document).find(R"(table:formula="of:=[$s.C1]+[.B1]")"),
            std::string::npos);
}

TEST(OdfSheetColumns, a_named_range_moves) {
  const Document document = document_of(flat_spreadsheet(
      abc(), R"(<table:named-expressions><table:named-range table:name="r")"
             R"( table:base-cell-address="$s.$A$1")"
             R"( table:cell-range-address="$s.$B$1:.$C$1"/>)"
             R"(</table:named-expressions>)"));

  sheet_at(document, 0).insert_columns(0, 1);

  EXPECT_NE(saved(document).find(R"(table:cell-range-address="$s.$C$1:.$D$1")"),
            std::string::npos);
}

TEST(OdfSheetColumns, an_edit_cutting_a_merge_refuses) {
  const Document document = document_of(flat_spreadsheet(
      table("s", row(R"(<table:table-cell table:number-columns-spanned="2"/>)"
                     "<table:covered-table-cell/>" +
                     string_cell("c")))));
  const Sheet sheet = sheet_at(document, 0);

  EXPECT_THROW(sheet.insert_columns(1, 1), UnsupportedOperation);
  EXPECT_THROW(sheet.delete_columns(1, 1), UnsupportedOperation);
  EXPECT_EQ(text_at(sheet, 2, 0), "c");

  sheet.insert_columns(2, 1);
  EXPECT_EQ(text_at(sheet, 3, 0), "c");
  sheet.delete_columns(0, 2);
  EXPECT_EQ(text_at(sheet, 1, 0), "c");
}

TEST(OdfSheetColumns, an_insert_pushing_a_cell_off_the_grid_refuses) {
  const Document document = document_of(
      flat_spreadsheet(table("s", row(empty_cells(16383) + string_cell("x")))));

  EXPECT_THROW(sheet_at(document, 0).insert_columns(0, 1),
               UnsupportedOperation);
}

TEST(OdfSheetColumns, the_ops_name_a_column_and_a_count) {
  const Document document = document_of(flat_spreadsheet(abc()));

  document.edit(
      R"({"version": 2, "ops": [)"
      R"({"op": "insertColumns", "sheet": 0, "column": 0, "count": 2},)"
      R"({"op": "deleteColumns", "sheet": 0, "column": 3, "count": 1}]})");

  const Sheet sheet = sheet_at(document, 0);
  EXPECT_EQ(text_at(sheet, 2, 0), "a");
  EXPECT_EQ(text_at(sheet, 3, 0), "c");
}

TEST(OdfSheetColumns, an_edited_sheet_saves_and_reopens) {
  const Document document = document_of(flat_spreadsheet(abc()));
  sheet_at(document, 0).insert_columns(1, 1);
  sheet_at(document, 0).delete_columns(0, 1);

  const Document reopened = document_of(saved(document));
  const Sheet sheet = sheet_at(reopened, 0);
  EXPECT_EQ(sheet.cell(0, 0).value().type(), ValueType::unknown);
  EXPECT_EQ(text_at(sheet, 1, 0), "b");
  EXPECT_EQ(text_at(sheet, 2, 0), "c");
}
