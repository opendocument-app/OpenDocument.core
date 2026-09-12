#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/open_strategy.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

/// A flat spreadsheet of @p sheets, each written as a `table:table`.
std::string flat_document(const std::string &sheets) {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.spreadsheet">)"
         R"(<office:body><office:spreadsheet>)" +
         sheets + R"(</office:spreadsheet></office:body></office:document>)";
}

std::string sheet_of(const std::string &name, const std::string &cells) {
  return R"(<table:table table:name=")" + name + R"("><table:table-row>)" +
         cells + R"(</table:table-row></table:table>)";
}

std::string number_cell(const std::string &text) {
  return R"(<table:table-cell office:value-type="float" office:value=")" +
         text + R"("><text:p>)" + text + R"(</text:p></table:table-cell>)";
}

/// A cell computing @p formula, with the result its producer cached.
std::string computed_cell(const std::string &formula,
                          const std::string &cached) {
  return R"(<table:table-cell table:formula=")" + formula +
         R"(" office:value-type="float" office:value=")" + cached +
         R"(" calcext:value-type="float"><text:p>)" + cached +
         R"(</text:p></table:table-cell>)";
}

Document document_of(const std::string &source) {
  return DecodedFile(
             open_strategy::open_file(std::make_shared<MemoryFile>(source), {},
                                      Logger::null()))
      .as_document_file()
      .document();
}

Sheet sheet_at(const Document &document, const std::uint32_t index) {
  Element element = *document.root_element().children().begin();
  for (std::uint32_t i = 0; i < index; ++i) {
    element = element.next_sibling();
  }
  return element.as_sheet();
}

/// One row: A1 a number, B1 the sum of A1 and nothing else, C1 twice B1.
std::string chain_sheet() {
  return sheet_of("s", number_cell("1") +
                           computed_cell("of:=SUM([.A1:.A1])", "1") +
                           computed_cell("of:=[.B1]*2", "2"));
}

} // namespace

/// ODF states no switch asking a reader to recompute, so the result goes: a
/// cell stating a formula and no result is one a reader has to compute.
TEST(OdfSheetStale, a_write_takes_the_result_of_what_reads_it_away) {
  const Document document = document_of(flat_document(chain_sheet()));
  const Sheet sheet = sheet_at(document, 0);

  sheet.set_cell(0, 0, CellValue(10.0, "10"));

  const CellValue stale = sheet.cell(1, 0).value();
  EXPECT_FALSE(stale.has_number());
  EXPECT_EQ(stale.text(), "");
  ASSERT_TRUE(stale.has_formula());
  EXPECT_EQ(stale.formula(), "of:=SUM([.A1:.A1])");
}

/// A formula reading a formula is wrong for the same reason.
TEST(OdfSheetStale, the_whole_chain_loses_its_results) {
  const Document document = document_of(flat_document(chain_sheet()));
  const Sheet sheet = sheet_at(document, 0);

  sheet.set_cell(0, 0, CellValue(10.0, "10"));

  EXPECT_FALSE(sheet.cell(2, 0).value().has_number());
  EXPECT_EQ(sheet.cell(2, 0).value().text(), "");
}

TEST(OdfSheetStale, a_formula_reading_something_else_keeps_its_result) {
  const Document document = document_of(
      flat_document(sheet_of("s", number_cell("1") + number_cell("2") +
                                      computed_cell("of:=[.B1]", "2"))));
  const Sheet sheet = sheet_at(document, 0);

  sheet.set_cell(0, 0, CellValue(10.0, "10"));

  EXPECT_DOUBLE_EQ(sheet.cell(2, 0).value().number(), 2);
  EXPECT_EQ(sheet.cell(2, 0).value().text(), "2");
}

TEST(OdfSheetStale, a_formula_on_another_sheet_loses_its_result_too) {
  const Document document = document_of(
      flat_document(sheet_of("Data", number_cell("1")) +
                    sheet_of("Report", computed_cell("of:=[Data.A1]*2", "2"))));

  sheet_at(document, 0).set_cell(0, 0, CellValue(10.0, "10"));

  EXPECT_FALSE(sheet_at(document, 1).cell(0, 0).value().has_number());
}

/// The cell keeps everything but the result, so a reader that computes one
/// shows it the way the file states.
TEST(OdfSheetStale, the_cell_keeps_its_style_and_its_formula) {
  const Document document = document_of(flat_document(sheet_of(
      "s", number_cell("1") +
               R"(<table:table-cell table:style-name="ce1")"
               R"( table:formula="of:=[.A1]" office:value-type="float")"
               R"( office:value="1"><text:p>1</text:p></table:table-cell>)")));
  const Sheet sheet = sheet_at(document, 0);

  sheet.set_cell(0, 0, CellValue(10.0, "10"));

  std::ostringstream saved;
  document.save(saved);

  EXPECT_NE(saved.str().find(R"(table:style-name="ce1")"), std::string::npos);
  EXPECT_NE(saved.str().find(R"(table:formula="of:=[.A1]")"),
            std::string::npos);
  EXPECT_EQ(saved.str().find(R"(office:value="1")"), std::string::npos);
}

/// Only the paragraph showing the result goes: a drawing anchored in the cell
/// is what the cell is.
TEST(OdfSheetStale, a_drawing_anchored_in_a_stale_cell_stays) {
  const Document document = document_of(flat_document(sheet_of(
      "s", number_cell("1") +
               R"(<table:table-cell table:formula="of:=[.A1]")"
               R"( office:value-type="float" office:value="1">)"
               R"(<text:p>1</text:p>)"
               R"(<draw:frame><draw:image xlink:href="x.png"/></draw:frame>)"
               R"(</table:table-cell>)")));

  sheet_at(document, 0).set_cell(0, 0, CellValue(10.0, "10"));

  std::ostringstream saved;
  document.save(saved);

  EXPECT_NE(saved.str().find("draw:frame"), std::string::npos);
  EXPECT_EQ(saved.str().find(R"(<text:p>1</text:p>)"), std::string::npos);
}

TEST(OdfSheetStale, the_saved_file_states_no_result_either) {
  const Document document = document_of(flat_document(chain_sheet()));
  sheet_at(document, 0).set_cell(0, 0, CellValue(10.0, "10"));

  std::ostringstream saved;
  document.save(saved);

  const Document reopened = document_of(saved.str());
  EXPECT_FALSE(sheet_at(reopened, 0).cell(1, 0).value().has_number());
  EXPECT_DOUBLE_EQ(sheet_at(reopened, 0).cell(0, 0).value().number(), 10);
}

/// Nothing reads the written cell, so no result is taken away.
TEST(OdfSheetStale, a_write_nothing_reads_leaves_every_result_alone) {
  const Document document = document_of(flat_document(
      sheet_of("s", number_cell("1") + computed_cell("of:=[.A1]", "1"))));
  const Sheet sheet = sheet_at(document, 0);

  sheet.set_cell(2, 0, CellValue(10.0, "10"));

  EXPECT_DOUBLE_EQ(sheet.cell(1, 0).value().number(), 1);
}
