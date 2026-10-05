#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/filesystem.hpp>
#include <odr/odr.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

CellValue value_of(const std::string &sheet_data) {
  const Document document = decode(workbook(sheet_data));
  return first_sheet(document).cell(0, 0).value();
}

std::string text_of(const std::string &sheet_data) {
  return value_of(sheet_data).text();
}

std::string formula_at(const std::string &sheet_data,
                       const std::uint32_t column, const std::uint32_t row) {
  const Document document = decode(workbook(sheet_data));
  return first_sheet(document).cell(column, row).value().formula();
}

} // namespace

/// ECMA-376 18.3.1.4: an inline string holds its text under `is`, one level
/// below the cell, which is where a shared one holds it too. The walker
/// descends into neither on its own, so the cell read as empty.
TEST(OoxmlSpreadsheetValue, an_inline_string_cell_reads_its_text) {
  EXPECT_EQ(
      text_of(R"(<row r="1"><c r="A1" t="inlineStr"><is><t>hello</t></is>)"
              R"(</c></row>)"),
      "hello");
}

TEST(OoxmlSpreadsheetValue, a_number_cell_reads_its_cached_text) {
  EXPECT_EQ(text_of(R"(<row r="1"><c r="A1"><v>42</v></c></row>)"), "42");
}

/// ECMA-376 18.3.1.4: `c/@t` defaults to "n", so a bare `<v>` is a number.
TEST(OoxmlSpreadsheetValue, a_number_cell_states_its_number) {
  const CellValue value =
      value_of(R"(<row r="1"><c r="A1"><v>12.5</v></c></row>)");

  EXPECT_EQ(value.type(), ValueType::float_number);
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 12.5);
  EXPECT_FALSE(value.has_formula());
}

TEST(OoxmlSpreadsheetValue, an_inline_string_cell_states_no_number) {
  const CellValue value = value_of(
      R"(<row r="1"><c r="A1" t="inlineStr"><is><t>12.5</t></is></c></row>)");

  EXPECT_EQ(value.type(), ValueType::string);
  EXPECT_FALSE(value.has_number());
}

/// The `<v>` is the result the producer cached; `<f>` is what computed it.
TEST(OoxmlSpreadsheetValue, a_formula_cell_states_both_formula_and_result) {
  const CellValue value =
      value_of(R"(<row r="1"><c r="A1"><f>SUM(B1:C1)</f><v>7</v></c></row>)");

  EXPECT_EQ(value.type(), ValueType::float_number);
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 7);
  ASSERT_TRUE(value.has_formula());
  EXPECT_EQ(value.formula(), "SUM(B1:C1)");
}

/// Set-and-empty says the member computes; unset would claim it does not.
TEST(OoxmlSpreadsheetValue,
     a_shared_formula_member_without_its_master_spells_nothing) {
  const CellValue value = value_of(
      R"(<row r="1"><c r="A1"><f t="shared" si="0"/><v>8</v></c></row>)");

  ASSERT_TRUE(value.has_formula());
  EXPECT_TRUE(value.formula().empty());
}

/// ECMA-376 18.3.1.40: the master spells the expression for the whole group.
TEST(OoxmlSpreadsheetValue, a_shared_formula_member_reads_its_master_moved) {
  const std::string data =
      R"(<row r="1"><c r="C1"><f t="shared" ref="C1:C3" si="0">A1+$B$1</f>)"
      R"(<v>3</v></c></row>)"
      R"(<row r="2"><c r="C2"><f t="shared" si="0"/><v>7</v></c></row>)"
      R"(<row r="3"><c r="C3"><f t="shared" si="0"/><v>9</v></c></row>)";

  EXPECT_EQ(formula_at(data, 2, 0), "A1+$B$1");
  EXPECT_EQ(formula_at(data, 2, 1), "A2+$B$1");
  EXPECT_EQ(formula_at(data, 2, 2), "A3+$B$1");
}

TEST(OoxmlSpreadsheetValue, a_shared_formula_member_can_lose_its_reference) {
  const std::string data =
      R"(<row r="2"><c r="C2"><f t="shared" ref="C1:C2" si="0">A1</f>)"
      R"(<v>3</v></c></row>)"
      R"(<row r="1"><c r="C1"><f t="shared" si="0"/><v>7</v></c></row>)";

  EXPECT_EQ(formula_at(data, 2, 0), "#REF!");
}

TEST(OoxmlSpreadsheetValue, a_shared_formula_that_does_not_parse_is_kept) {
  const std::string data =
      R"(<row r="1"><c r="C1"><f t="shared" ref="C1:C2" si="0">A1 +</f>)"
      R"(<v>3</v></c></row>)"
      R"(<row r="2"><c r="C2"><f t="shared" si="0"/><v>7</v></c></row>)";

  EXPECT_EQ(formula_at(data, 2, 1), "A1 +");
}

/// ECMA-376 18.18.11: `c/@t="b"` types the cell a boolean, `<v>` its 1 or 0.
TEST(OoxmlSpreadsheetValue, a_boolean_cell_is_typed_and_states_one_or_zero) {
  const CellValue value =
      value_of(R"(<row r="1"><c r="A1" t="b"><v>1</v></c></row>)");

  EXPECT_EQ(value.type(), ValueType::boolean);
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 1);
}

/// An error cell states its text and a date cell its ISO 8601 text; neither
/// is a number.
TEST(OoxmlSpreadsheetValue, an_error_and_a_date_cell_are_typed) {
  const CellValue error =
      value_of(R"(<row r="1"><c r="A1" t="e"><v>#DIV/0!</v></c></row>)");
  EXPECT_EQ(error.type(), ValueType::error);
  EXPECT_FALSE(error.has_number());

  const CellValue date =
      value_of(R"(<row r="1"><c r="A1" t="d"><v>2024-01-31</v></c></row>)");
  EXPECT_EQ(date.type(), ValueType::date);
  EXPECT_FALSE(date.has_number());
}

namespace {

/// `cellXfs` 1 to 5: a custom `#,##0.00`, the built-in date 14, percent 9,
/// time 20, and a code this cannot read.
constexpr const char *number_styles =
    R"(<numFmts count="2"><numFmt numFmtId="164" formatCode="#,##0.00"/>)"
    R"(<numFmt numFmtId="165" formatCode="&quot;open"/></numFmts>)"
    R"(<fonts count="1"><font><sz val="11"/><name val="Calibri"/></font></fonts>)"
    R"(<fills count="2"><fill><patternFill patternType="none"/></fill>)"
    R"(<fill><patternFill patternType="gray125"/></fill></fills>)"
    R"(<borders count="1"><border/></borders>)"
    R"(<cellStyleXfs count="1"><xf numFmtId="0" fontId="0" fillId="0" borderId="0"/></cellStyleXfs>)"
    R"(<cellXfs count="6"><xf numFmtId="0" fontId="0" fillId="0" borderId="0" xfId="0"/>)"
    R"(<xf numFmtId="164" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/>)"
    R"(<xf numFmtId="14" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/>)"
    R"(<xf numFmtId="9" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/>)"
    R"(<xf numFmtId="20" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/>)"
    R"(<xf numFmtId="165" fontId="0" fillId="0" borderId="0" xfId="0" applyNumberFormat="1"/>)"
    R"(</cellXfs>)";

Document formatted(const std::string &workbook_extra = "") {
  return decode(workbook(
      R"(<row r="1"><c r="A1" s="1"><v>1234.5</v></c>)"
      R"(<c r="B1" s="2"><v>45658</v></c><c r="C1" s="3"><v>0.25</v></c>)"
      R"(<c r="D1" s="4"><v>0.75</v></c><c r="E1" s="5"><v>0.1</v></c>)"
      R"(<c r="F1" t="b"><v>1</v></c><c r="G1"><v>0.30000000000000004</v></c>)"
      R"(</row>)",
      "", "", workbook_extra, "", number_styles));
}

std::string shown_at(const Sheet &sheet, const std::uint32_t column) {
  return sheet.cell(column, 0).first_child().as_text().content();
}

} // namespace

TEST(OoxmlSpreadsheetValue, a_number_shows_its_format) {
  const Document document = formatted();
  const Sheet sheet = first_sheet(document);

  EXPECT_EQ(shown_at(sheet, 0), "1,234.50");
  EXPECT_EQ(shown_at(sheet, 1), "01-01-25");
  EXPECT_EQ(shown_at(sheet, 2), "25%");
  EXPECT_EQ(shown_at(sheet, 3), "18:00");
  EXPECT_EQ(shown_at(sheet, 4), "0.1");
  EXPECT_EQ(shown_at(sheet, 5), "TRUE");
  EXPECT_EQ(shown_at(sheet, 6), "0.3");

  const CellValue value = sheet.cell(0, 0).value();
  EXPECT_EQ(value.type(), ValueType::float_number);
  EXPECT_DOUBLE_EQ(value.number(), 1234.5);
  EXPECT_EQ(value.text(), "1,234.50");
}

TEST(OoxmlSpreadsheetValue, a_date_format_types_a_date) {
  const Document document = formatted();
  const Sheet sheet = first_sheet(document);

  const CellValue date = sheet.cell(1, 0).value();
  EXPECT_EQ(date.type(), ValueType::date);
  EXPECT_DOUBLE_EQ(date.number(), 45658);
  EXPECT_EQ(date.text(), "01-01-25");
  EXPECT_EQ(sheet.cell(3, 0).value().type(), ValueType::time);
  EXPECT_DOUBLE_EQ(sheet.cell(3, 0).value().number(), 0.75);
  EXPECT_EQ(sheet.cell(2, 0).value().type(), ValueType::float_number);
}

TEST(OoxmlSpreadsheetValue, date1904_counts_from_1904) {
  const Document document = formatted(R"(<workbookPr date1904="1"/>)");
  const Sheet sheet = first_sheet(document);

  EXPECT_EQ(shown_at(sheet, 1), "01-02-29");
  // the value counts from 1899-12-30, whatever the workbook does
  EXPECT_DOUBLE_EQ(sheet.cell(1, 0).value().number(), 45658 + 1462);
}

TEST(OoxmlSpreadsheetValue, a_date_takes_the_names_of_its_format_language) {
  const std::string styles =
      std::string(number_styles)
          .replace(std::string(number_styles).find("#,##0.00"), 8,
                   "[$-419]d mmmm yyyy");
  const Document document =
      decode(workbook(R"(<row r="1"><c r="A1" s="1"><v>45731</v></c></row>)",
                      "", "", "", "", styles));

  EXPECT_EQ(shown_at(first_sheet(document), 0), "15 марта 2025");
}

namespace {

Document saved_and_reopened(const Document &document) {
  std::ostringstream saved;
  document.save(saved);
  return odr::open(odr::File::from_memory(saved.str()))
      .as_document_file()
      .document();
}

CellValue date(const double days) {
  return CellValue(ValueType::date).with_number(days);
}

} // namespace

TEST(OoxmlSpreadsheetValue, a_written_date_gets_a_date_format) {
  const Document document = formatted();
  const Sheet sheet = first_sheet(document);

  sheet.set_cell(6, 0, date(45658));
  sheet.set_cell(7, 0, date(45658.75));
  sheet.set_cell(8, 0, CellValue(ValueType::time).with_number(0.75));
  sheet.set_cell(9, 0,
                 CellValue(ValueType::time).with_number(0.75 + 5.0 / 86400));
  sheet.set_cell(10, 0, CellValue(ValueType::time).with_number(1.75));

  const Document reopened = saved_and_reopened(document);
  const Sheet saved = first_sheet(reopened);
  EXPECT_EQ(saved.cell(6, 0).value().type(), ValueType::date);
  EXPECT_DOUBLE_EQ(saved.cell(6, 0).value().number(), 45658);
  EXPECT_EQ(shown_at(saved, 6), "01-01-25");
  EXPECT_EQ(shown_at(saved, 7), "1/1/25 18:00");
  EXPECT_EQ(saved.cell(8, 0).value().type(), ValueType::time);
  EXPECT_EQ(shown_at(saved, 8), "18:00");
  EXPECT_EQ(shown_at(saved, 9), "18:00:05");
  // a time of a day or more does not wrap
  EXPECT_EQ(shown_at(saved, 10), "42:00:00");
}

TEST(OoxmlSpreadsheetValue, a_date_keeps_the_date_format_its_cell_has) {
  const Document document = formatted(R"(<workbookPr date1904="1"/>)");
  const Sheet sheet = first_sheet(document);

  // `B1` shows `cellXfs` 2, the built-in date 14
  sheet.set_cell(1, 0, date(45659));

  const Document reopened = saved_and_reopened(document);
  const Sheet saved = first_sheet(reopened);
  EXPECT_EQ(shown_at(saved, 1), "01-02-25");
  EXPECT_DOUBLE_EQ(saved.cell(1, 0).value().number(), 45659);
  std::ostringstream xml;
  xml << reopened.as_filesystem()
             .open("/xl/worksheets/sheet1.xml")
             .stream()
             ->rdbuf();
  // 1904 counts 1462 days fewer
  EXPECT_NE(xml.str().find(R"(<c r="B1" s="2"><v>44197</v>)"),
            std::string::npos)
      << xml.str();
}

TEST(OoxmlSpreadsheetValue, omitted_coordinates_follow_the_previous_entry) {
  const Document document = decode(workbook(
      R"(<row><c><v>1</v></c><c r="C1"><v>3</v></c><c><v>4</v></c></row>)"
      R"(<row r="4"><c><v>5</v></c></row><row><c><v>6</v></c></row>)"));
  const Sheet sheet = first_sheet(document);
  EXPECT_DOUBLE_EQ(sheet.cell(0, 0).value().number(), 1);
  EXPECT_DOUBLE_EQ(sheet.cell(2, 0).value().number(), 3);
  EXPECT_DOUBLE_EQ(sheet.cell(3, 0).value().number(), 4);
  EXPECT_DOUBLE_EQ(sheet.cell(0, 3).value().number(), 5);
  EXPECT_DOUBLE_EQ(sheet.cell(0, 4).value().number(), 6);
  sheet.insert_rows(1, 1);
  sheet.set_cell(3, 0, CellValue(7));
  const Document saved = saved_and_reopened(document);
  EXPECT_DOUBLE_EQ(first_sheet(saved).cell(3, 0).value().number(), 7);
  EXPECT_DOUBLE_EQ(first_sheet(saved).cell(0, 4).value().number(), 5);
  EXPECT_DOUBLE_EQ(first_sheet(saved).cell(0, 5).value().number(), 6);
}

TEST(OoxmlSpreadsheetValue, an_invalid_index_keeps_the_rest_of_the_sheet) {
  for (const std::string index :
       {"", "-1", "1x", "4294967296", "18446744073709551616"}) {
    SCOPED_TRACE(index);
    const Document document = decode(workbook(
        R"(<row r="1"><c><v>1</v></c></row><row r=")" + index +
            R"("><c><v>2</v></c><c t="s"><v>)" + index + "</v></c></row>",
        "", "<si><t>zero</t></si><si><t>one</t></si>", "",
        R"(<cols><col min="1" max=")" + index + R"("/><col min=")" + index +
            R"(" max="1"/></cols>)"));
    const Sheet sheet = first_sheet(document);
    EXPECT_DOUBLE_EQ(sheet.cell(0, 1).value().number(), 2);
    EXPECT_FALSE(sheet.cell(1, 1).first_child());
  }
  EXPECT_NO_THROW(decode(workbook("", "", "", "",
                                  R"(<cols><col min="0" max="1"/>)"
                                  R"(<col min="2" max="1"/></cols>)")));
  EXPECT_DOUBLE_EQ(
      first_sheet(
          decode(workbook(R"(<row r="1"/><row r="0"><c><v>2</v></c></row>)")))
          .cell(0, 1)
          .value()
          .number(),
      2);
  const Document document =
      decode(workbook(R"(<row r=" +1 "><c r="A1" t="s"><v> +0 </v></c></row>)",
                      "", "<si><t>zero</t></si>"));
  EXPECT_EQ(first_sheet(document).cell(0, 0).value().text(), "zero");
}
