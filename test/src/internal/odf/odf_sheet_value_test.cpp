#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/open_strategy.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

/// A flat sheet holding one cell, written as @p cell.
std::string flat_sheet(const std::string &cell) {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.spreadsheet">)"
         R"(<office:body><office:spreadsheet>)"
         R"(<table:table table:name="s"><table:table-row>)" +
         cell +
         R"(</table:table-row></table:table>)"
         R"(</office:spreadsheet></office:body></office:document>)";
}

CellValue value_of(const std::string &cell) {
  const Document document =
      DecodedFile(open_strategy::open_file(
                      std::make_shared<MemoryFile>(flat_sheet(cell)), {},
                      Logger::null()))
          .as_document_file()
          .document();
  const Sheet sheet = (*document.root_element().children().begin()).as_sheet();
  return sheet.cell(0, 0).value();
}

} // namespace

/// [ODF 1.2] 19.386: the number is `office:value`; the `text:p` beside it is
/// the producer's formatting of it.
TEST(OdfSheetValue, a_float_cell_states_its_number) {
  const CellValue value = value_of(
      R"(<table:table-cell office:value-type="float" office:value="1234.5">)"
      R"(<text:p>1 234,50</text:p></table:table-cell>)");

  EXPECT_EQ(value.type(), ValueType::float_number);
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 1234.5);
  EXPECT_FALSE(value.has_formula());
}

TEST(OdfSheetValue, a_string_cell_states_no_number) {
  const CellValue value =
      value_of(R"(<table:table-cell office:value-type="string">)"
               R"(<text:p>1234.5</text:p></table:table-cell>)");

  EXPECT_EQ(value.type(), ValueType::string);
  EXPECT_FALSE(value.has_number());
}

/// [ODF 1.2] 19.642 `table:formula`, whose namespace prefix is the syntax it
/// is written in and stays part of the string.
TEST(OdfSheetValue, a_formula_cell_states_both_formula_and_result) {
  // a `)"` inside the attribute would close a default-delimited raw string
  const CellValue value =
      value_of(R"xml(<table:table-cell table:formula="of:=SUM([.B1:.C1])")xml"
               R"( office:value-type="float" office:value="7">)"
               R"(<text:p>7</text:p></table:table-cell>)");

  ASSERT_TRUE(value.has_formula());
  EXPECT_EQ(value.formula(), "of:=SUM([.B1:.C1])");
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 7);
}

/// The type is read from `office:value-type` alone; reading the number format
/// is what would settle it.
TEST(OdfSheetValue, a_percentage_states_a_number_the_type_does_not_admit) {
  const CellValue value = value_of(
      R"(<table:table-cell office:value-type="percentage" office:value="0.25">)"
      R"(<text:p>25%</text:p></table:table-cell>)");

  EXPECT_EQ(value.type(), ValueType::string);
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 0.25);
}

TEST(OdfSheetValue, a_number_is_read_in_one_spelling_only) {
  const CellValue value = value_of(
      R"(<table:table-cell office:value-type="float" office:value="1234,5">)"
      R"(<text:p>1234,5</text:p></table:table-cell>)");

  EXPECT_FALSE(value.has_number());
}
