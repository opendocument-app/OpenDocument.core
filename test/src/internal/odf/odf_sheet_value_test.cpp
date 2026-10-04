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

/// A percentage and a currency state their number in `office:value`, as a
/// float does; the data style is what shows them.
TEST(OdfSheetValue, a_percentage_and_a_currency_are_numbers) {
  const CellValue percentage = value_of(
      R"(<table:table-cell office:value-type="percentage" office:value="0.25">)"
      R"(<text:p>25%</text:p></table:table-cell>)");
  const CellValue currency = value_of(
      R"(<table:table-cell office:value-type="currency" office:currency="EUR")"
      R"( office:value="1234.5"><text:p>1.234,50 €</text:p></table:table-cell>)");

  EXPECT_EQ(percentage.type(), ValueType::float_number);
  EXPECT_DOUBLE_EQ(percentage.number(), 0.25);
  EXPECT_EQ(currency.type(), ValueType::float_number);
  EXPECT_DOUBLE_EQ(currency.number(), 1234.5);
}

TEST(OdfSheetValue, a_number_is_read_in_one_spelling_only) {
  const CellValue value = value_of(
      R"(<table:table-cell office:value-type="float" office:value="1234,5">)"
      R"(<text:p>1234,5</text:p></table:table-cell>)");

  EXPECT_FALSE(value.has_number());
}

/// [ODF 1.2] 19.385: a boolean states `office:boolean-value`, read as 1 or 0.
TEST(OdfSheetValue, a_boolean_cell_is_typed_and_states_one_or_zero) {
  const CellValue value = value_of(
      R"(<table:table-cell office:value-type="boolean" office:boolean-value="true">)"
      R"(<text:p>TRUE</text:p></table:table-cell>)");

  EXPECT_EQ(value.type(), ValueType::boolean);
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 1);
}

TEST(OdfSheetValue, a_date_and_a_time_state_days_since_1899_12_30) {
  const CellValue date =
      value_of(R"(<table:table-cell office:value-type="date")"
               R"( office:date-value="2025-01-01"><text:p>01/01/25</text:p>)"
               R"(</table:table-cell>)");
  const CellValue date_time =
      value_of(R"(<table:table-cell office:value-type="date")"
               R"( office:date-value="2025-01-01T18:00:00"><text:p>x</text:p>)"
               R"(</table:table-cell>)");
  const CellValue time =
      value_of(R"(<table:table-cell office:value-type="time")"
               R"( office:time-value="PT18H30M00S"><text:p>18:30</text:p>)"
               R"(</table:table-cell>)");
  const CellValue elapsed =
      value_of(R"(<table:table-cell office:value-type="time")"
               R"( office:time-value="P1DT12H"><text:p>36:00</text:p>)"
               R"(</table:table-cell>)");

  EXPECT_EQ(date.type(), ValueType::date);
  EXPECT_DOUBLE_EQ(date.number(), 45658);
  EXPECT_DOUBLE_EQ(date_time.number(), 45658.75);
  EXPECT_EQ(time.type(), ValueType::time);
  EXPECT_DOUBLE_EQ(time.number(), 18.5 / 24);
  EXPECT_DOUBLE_EQ(elapsed.number(), 1.5);
}

TEST(OdfSheetValue, a_date_that_does_not_parse_states_no_number) {
  for (const char *date : {"soon", "2025-13-01", "2025-01-45"}) {
    EXPECT_FALSE(value_of(std::string(R"(<table:table-cell)"
                                      R"( office:value-type="date")"
                                      R"( office:date-value=")") +
                          date + R"("><text:p>x</text:p></table:table-cell>)")
                     .has_number())
        << date;
  }
}
