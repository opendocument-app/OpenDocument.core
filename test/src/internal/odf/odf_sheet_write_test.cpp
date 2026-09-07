#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

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

/// A flat sheet whose single row holds @p cells.
std::string flat_sheet(const std::string &cells) {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.spreadsheet">)"
         R"(<office:body><office:spreadsheet>)"
         R"(<table:table table:name="s"><table:table-row>)" +
         cells +
         R"(</table:table-row></table:table>)"
         R"(</office:spreadsheet></office:body></office:document>)";
}

std::string string_cell(const std::string &text) {
  return R"(<table:table-cell office:value-type="string"><text:p>)" + text +
         R"(</text:p></table:table-cell>)";
}

Document document_of(const std::string &source) {
  return DecodedFile(
             open_strategy::open_file(std::make_shared<MemoryFile>(source), {},
                                      Logger::null()))
      .as_document_file()
      .document();
}

Sheet first_sheet(const Document &document) {
  return (*document.root_element().children().begin()).as_sheet();
}

} // namespace

/// [ODF 1.2] 19.385: the cell states the value and the `text:p` shows it.
TEST(OdfSheetWrite, a_number_is_written_as_both_value_and_text) {
  const Document document = document_of(flat_sheet(string_cell("old")));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 0, CellValue(1234.5, "1 234,50"));

  const CellValue value = sheet.cell(0, 0).value();
  EXPECT_EQ(value.type(), ValueType::float_number);
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 1234.5);
  EXPECT_EQ(sheet.cell(0, 0).value().text(), "1 234,50");
}

TEST(OdfSheetWrite, a_string_written_over_a_number_takes_the_number_away) {
  const Document document = document_of(flat_sheet(
      R"(<table:table-cell office:value-type="float" office:value="7">)"
      R"(<text:p>7</text:p></table:table-cell>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 0, CellValue("seven"));

  const CellValue value = sheet.cell(0, 0).value();
  EXPECT_EQ(value.type(), ValueType::string);
  EXPECT_FALSE(value.has_number());
  EXPECT_EQ(sheet.cell(0, 0).value().text(), "seven");
}

TEST(OdfSheetWrite, a_cleared_cell_states_nothing) {
  const Document document = document_of(flat_sheet(
      R"(<table:table-cell office:value-type="float" office:value="7">)"
      R"(<text:p>7</text:p></table:table-cell>)"));
  const Sheet sheet = first_sheet(document);

  sheet.clear_cell(0, 0);

  const CellValue value = sheet.cell(0, 0).value();
  EXPECT_EQ(value.type(), ValueType::string); // no `office:value-type` left
  EXPECT_FALSE(value.has_number());
  EXPECT_EQ(sheet.cell(0, 0).value().text(), "");
}

/// One element stands for every cell of the run, so a write cuts it in three.
TEST(OdfSheetWrite, a_repeated_cell_is_split_by_a_write) {
  const Document document = document_of(flat_sheet(
      R"(<table:table-cell table:number-columns-repeated="4")"
      R"( office:value-type="string"><text:p>x</text:p></table:table-cell>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(2, 0, CellValue("y"));

  EXPECT_EQ(sheet.cell(0, 0).value().text(), "x");
  EXPECT_EQ(sheet.cell(1, 0).value().text(), "x");
  EXPECT_EQ(sheet.cell(2, 0).value().text(), "y");
  EXPECT_EQ(sheet.cell(3, 0).value().text(), "x");
}

TEST(OdfSheetWrite, a_repeat_is_split_at_either_end_too) {
  const Document document = document_of(flat_sheet(
      R"(<table:table-cell table:number-columns-repeated="3")"
      R"( office:value-type="string"><text:p>x</text:p></table:table-cell>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 0, CellValue("a"));
  sheet.set_cell(2, 0, CellValue("c"));

  EXPECT_EQ(sheet.cell(0, 0).value().text(), "a");
  EXPECT_EQ(sheet.cell(1, 0).value().text(), "x");
  EXPECT_EQ(sheet.cell(2, 0).value().text(), "c");
}

/// The refusals are decided on the run, so one leaves it uncut.
TEST(OdfSheetWrite, a_refused_write_leaves_a_repeat_uncut) {
  const Document document = document_of(flat_sheet(
      R"(<table:table-cell table:number-columns-repeated="4")"
      R"xml( table:formula="of:=SUM([.A2:.B2])" office:value-type="float")xml"
      R"( office:value="7"><text:p>7</text:p></table:table-cell>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(2, 0, CellValue("y")), UnsupportedOperation);

  std::ostringstream saved;
  document.save(saved);
  EXPECT_NE(saved.str().find(R"(table:number-columns-repeated="4")"),
            std::string::npos);
}

/// A repeated row is cut the same way, so only the row written changes.
TEST(OdfSheetWrite, a_repeated_row_is_split_by_a_write) {
  const Document document = document_of(
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.spreadsheet">)"
      R"(<office:body><office:spreadsheet>)"
      R"(<table:table table:name="s">)"
      R"(<table:table-row table:number-rows-repeated="3">)"
      R"(<table:table-cell table:number-columns-repeated="2")"
      R"( office:value-type="string"><text:p>x</text:p></table:table-cell>)"
      R"(</table:table-row></table:table>)"
      R"(</office:spreadsheet></office:body></office:document>)");
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(1, 1, CellValue("y"));

  EXPECT_EQ(sheet.cell(1, 1).value().text(), "y");
  EXPECT_EQ(sheet.cell(0, 1).value().text(), "x");
  for (const std::uint32_t row : {0u, 2u}) {
    EXPECT_EQ(sheet.cell(0, row).value().text(), "x") << row;
    EXPECT_EQ(sheet.cell(1, row).value().text(), "x") << row;
  }
  EXPECT_EQ(sheet.dimensions().rows, 3);
}

/// The id names a position, so the handle still means its cell once the run
/// is cut under it.
TEST(OdfSheetWrite, a_handle_taken_before_a_split_shows_the_write) {
  const Document document = document_of(flat_sheet(
      R"(<table:table-cell table:number-columns-repeated="4")"
      R"( office:value-type="string"><text:p>x</text:p></table:table-cell>)"));
  const Sheet sheet = first_sheet(document);

  const SheetCell held = sheet.cell(2, 0);
  sheet.set_cell(2, 0, CellValue("y"));

  EXPECT_EQ(held.value().text(), "y");
  EXPECT_EQ(held.position().column, 2);
}

TEST(OdfSheetWrite, a_split_sheet_saves_and_reopens) {
  const Document document = document_of(flat_sheet(
      R"(<table:table-cell table:number-columns-repeated="4")"
      R"( office:value-type="string"><text:p>x</text:p></table:table-cell>)"));
  first_sheet(document).set_cell(2, 0, CellValue(41.5, "41.5"));

  std::ostringstream saved;
  document.save(saved);
  const Document reopened = document_of(saved.str());
  const Sheet sheet = first_sheet(reopened);

  EXPECT_EQ(sheet.cell(1, 0).value().text(), "x");
  ASSERT_TRUE(sheet.cell(2, 0).value().has_number());
  EXPECT_DOUBLE_EQ(sheet.cell(2, 0).value().number(), 41.5);
  EXPECT_EQ(sheet.cell(3, 0).value().text(), "x");
}

TEST(OdfSheetWrite, a_formula_cell_refuses_to_be_written) {
  const Document document = document_of(
      flat_sheet(R"xml(<table:table-cell table:formula="of:=SUM([.B1:.C1])")xml"
                 R"( office:value-type="float" office:value="7">)"
                 R"(<text:p>7</text:p></table:table-cell>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(0, 0, CellValue("y")), UnsupportedOperation);
}

/// An empty cell is written as no element at all.
TEST(OdfSheetWrite, an_absent_cell_refuses_to_be_written) {
  const Document document = document_of(flat_sheet(string_cell("a")));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(4, 4, CellValue("y")), UnsupportedOperation);
}

TEST(OdfSheetWrite, a_cell_of_several_paragraphs_refuses_to_be_written) {
  const Document document = document_of(
      flat_sheet(R"(<table:table-cell office:value-type="string">)"
                 R"(<text:p>a</text:p><text:p>b</text:p></table:table-cell>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(0, 0, CellValue("y")), UnsupportedOperation);
}

/// A blank cell that carries a style is written as an empty `text:p`, which is
/// also what clearing one leaves behind, so it has to stay writable.
TEST(OdfSheetWrite, a_cell_of_an_empty_paragraph_is_written_through) {
  const Document document =
      document_of(flat_sheet(R"(<table:table-cell office:value-type="string">)"
                             R"(<text:p/></table:table-cell>)"));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(0, 0, CellValue("y"));

  EXPECT_EQ(sheet.cell(0, 0).value().text(), "y");
}

TEST(OdfSheetWrite, a_cleared_cell_is_written_again_after_a_reopen) {
  const Document document = document_of(flat_sheet(string_cell("old")));
  first_sheet(document).clear_cell(0, 0);

  std::ostringstream saved;
  document.save(saved);
  const Document reopened = document_of(saved.str());
  first_sheet(reopened).set_cell(0, 0, CellValue("new"));

  EXPECT_EQ(first_sheet(reopened).cell(0, 0).value().text(), "new");
}

/// What a cell reads as is what writing it back takes, which is the point of
/// the one type.
TEST(OdfSheetWrite, a_value_read_out_of_a_cell_writes_into_another) {
  const Document document = document_of(
      flat_sheet(R"(<table:table-cell office:value-type="float")"
                 R"( office:value="1234.5"><text:p>1 234,50</text:p>)"
                 R"(</table:table-cell>)" +
                 string_cell("other")));
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(1, 0, sheet.cell(0, 0).value());

  const CellValue value = sheet.cell(1, 0).value();
  EXPECT_EQ(value.type(), ValueType::float_number);
  EXPECT_DOUBLE_EQ(value.number(), 1234.5);
  EXPECT_EQ(value.text(), "1 234,50");
}

/// Writing one waits for an evaluator, so a formula cell's value cannot be
/// handed back either.
TEST(OdfSheetWrite, a_value_holding_a_formula_refuses_to_be_written) {
  const Document document = document_of(flat_sheet(string_cell("a")));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(
                   0, 0, CellValue(7, "7").with_formula("of:=SUM([.B1:.C1])")),
               UnsupportedOperation);
}

TEST(OdfSheetWrite, a_written_sheet_saves_and_reopens) {
  const Document document = document_of(flat_sheet(string_cell("old")));
  ASSERT_TRUE(document.is_editable());
  ASSERT_TRUE(document.is_savable());

  first_sheet(document).set_cell(0, 0, CellValue(41.5, "41.5"));

  std::ostringstream saved;
  document.save(saved);

  const Document reopened = document_of(saved.str());
  const CellValue value = first_sheet(reopened).cell(0, 0).value();

  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 41.5);
  EXPECT_EQ(first_sheet(reopened).cell(0, 0).value().text(), "41.5");
}

/// Every refusal is decided before anything is written.
TEST(OdfSheetWrite, a_number_stating_none_leaves_the_cell_alone) {
  const Document document = document_of(flat_sheet(string_cell("old")));
  const Sheet sheet = first_sheet(document);

  EXPECT_THROW(sheet.set_cell(0, 0, CellValue(ValueType::float_number)),
               ValueNotStated);
  EXPECT_EQ(sheet.cell(0, 0).value().text(), "old");
}
