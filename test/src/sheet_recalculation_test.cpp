#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/sheet_position.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/open_strategy.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::internal;

namespace {

Document decode(const std::shared_ptr<abstract::File> &file) {
  return DecodedFile(open_strategy::open_file(file, {}, Logger::null()))
      .as_document_file()
      .document();
}

/// A flat spreadsheet of one sheet `s` holding @p rows.
Document ods(const std::string &rows) {
  return decode(std::make_shared<MemoryFile>(
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.spreadsheet">)"
      R"(<office:body><office:spreadsheet><table:table table:name="s">)" +
      rows +
      R"(</table:table></office:spreadsheet></office:body></office:document>)"));
}

std::string row(const std::string &cells) {
  return "<table:table-row>" + cells + "</table:table-row>";
}

std::string number(const std::string &text) {
  return R"(<table:table-cell office:value-type="float" office:value=")" +
         text + R"("><text:p>)" + text + R"(</text:p></table:table-cell>)";
}

/// A formula cell caching @p result as a number.
std::string computed(const std::string &formula, const std::string &result) {
  return R"(<table:table-cell table:formula=")" + formula +
         R"(" office:value-type="float" office:value=")" + result +
         R"("><text:p>)" + result + R"(</text:p></table:table-cell>)";
}

/// A formula cell stating no result.
std::string uncomputed(const std::string &formula) {
  return R"(<table:table-cell table:formula=")" + formula + R"("/>)";
}

Document reloaded(const Document &document) {
  return decode(std::make_shared<MemoryFile>(
      std::string(document.save_to_memory().memory_data().value())));
}

Sheet first_sheet(const Document &document) {
  return (*document.root_element().children().begin()).as_sheet();
}

CellValue value_at(const Document &document, const std::uint32_t column,
                   const std::uint32_t row) {
  return first_sheet(document).cell(column, row).value();
}

std::vector<std::string> spelled(const std::vector<SheetPosition> &positions) {
  std::vector<std::string> result;
  for (const SheetPosition &position : positions) {
    result.push_back(position.to_string());
  }
  return result;
}

} // namespace

TEST(SheetRecalculation, an_edit_computes_what_reads_it) {
  const Document document = ods(row(number("1") + computed("of:=[.A1]*2", "2") +
                                    computed("of:=[.B1]+1", "3")));

  first_sheet(document).set_cell(0, 0, CellValue(5));
  const Recalculation result = document.recalculate();

  EXPECT_EQ(spelled(result.changed()),
            (std::vector<std::string>{"0!B1", "0!C1"}));
  EXPECT_TRUE(result.circular().empty());
  EXPECT_TRUE(result.unevaluated().empty());
  EXPECT_EQ(value_at(document, 1, 0).number(), 10);
  EXPECT_EQ(value_at(document, 1, 0).text(), "10");
  EXPECT_EQ(value_at(document, 2, 0).number(), 11);
  EXPECT_EQ(value_at(document, 1, 0).formula(), "of:=[.A1]*2");
}

TEST(SheetRecalculation, a_second_recalculation_finds_nothing_stale) {
  const Document document =
      ods(row(number("1") + computed("of:=[.A1]*2", "2")));
  first_sheet(document).set_cell(0, 0, CellValue(5));
  EXPECT_EQ(document.recalculate().changed().size(), 1);

  EXPECT_TRUE(document.recalculate().changed().empty());
}

TEST(SheetRecalculation, a_formula_without_a_result_is_computed) {
  const Document document = ods(row(number("4") + uncomputed("of:=[.A1]/2")));

  EXPECT_EQ(spelled(document.recalculate().changed()),
            (std::vector<std::string>{"0!B1"}));
  EXPECT_EQ(value_at(document, 1, 0).number(), 2);
}

TEST(SheetRecalculation, a_result_keeps_its_type) {
  const Document document =
      ods(row(number("1") + uncomputed(R"(of:=&quot;a&quot;&amp;[.A1])") +
              uncomputed("of:=[.A1]&gt;0") + uncomputed("of:=1/0")));
  document.recalculate();

  EXPECT_EQ(value_at(document, 1, 0).type(), ValueType::string);
  EXPECT_EQ(value_at(document, 1, 0).text(), "a1");
  EXPECT_EQ(value_at(document, 2, 0).type(), ValueType::boolean);
  EXPECT_EQ(value_at(document, 2, 0).text(), "TRUE");
  EXPECT_EQ(value_at(document, 3, 0).type(), ValueType::error);
  EXPECT_EQ(value_at(document, 3, 0).text(), "#DIV/0!");

  const Document saved = reloaded(document);
  EXPECT_EQ(value_at(saved, 1, 0).text(), "a1");
  EXPECT_EQ(value_at(saved, 3, 0).type(), ValueType::error);
}

TEST(SheetRecalculation, a_cycle_gets_no_result) {
  const Document document =
      ods(row(uncomputed("of:=[.B1]+1") + uncomputed("of:=[.A1]+1") +
              uncomputed("of:=[.A1]*2")));
  const Recalculation result = document.recalculate();

  EXPECT_EQ(spelled(result.circular()),
            (std::vector<std::string>{"0!A1", "0!B1"}));
  EXPECT_EQ(spelled(result.unevaluated()), (std::vector<std::string>{"0!C1"}));
  EXPECT_TRUE(result.changed().empty());
}

TEST(SheetRecalculation, what_the_evaluator_does_not_know_stays_without_one) {
  const Document document =
      ods(row(number("1") + uncomputed("of:=NOSUCHFUNCTION([.A1])") +
              uncomputed("of:=[.B1]+1")));
  const Recalculation result = document.recalculate();

  EXPECT_EQ(spelled(result.unevaluated()),
            (std::vector<std::string>{"0!B1", "0!C1"}));
  EXPECT_EQ(value_at(document, 2, 0).type(), ValueType::unknown);
}

TEST(SheetRecalculation, a_running_total_down_a_column) {
  std::string rows = row(number("1"));
  for (int i = 2; i <= 2000; ++i) {
    rows += row(uncomputed("of:=[.A" + std::to_string(i - 1) + "]+1"));
  }
  const Document document = ods(rows);

  EXPECT_EQ(document.recalculate().changed().size(), 1999);
  EXPECT_EQ(value_at(document, 0, 1999).number(), 2000);
}

TEST(SheetRecalculation, a_chain_up_a_column_past_the_depth_limit) {
  std::string rows;
  for (int i = 1; i < 1100; ++i) {
    rows += row(uncomputed("of:=[.A" + std::to_string(i + 1) + "]+1"));
  }
  rows += row(number("1"));
  const Document document = ods(rows);

  EXPECT_EQ(document.recalculate().changed().size(), 1099);
  EXPECT_EQ(value_at(document, 0, 0).number(), 1100);
}

TEST(SheetRecalculation, a_cycle_longer_than_the_depth_limit_gets_no_result) {
  std::string rows;
  for (int i = 1; i <= 200; ++i) {
    rows += row(uncomputed("of:=[.A" + std::to_string(i % 200 + 1) + "]+1"));
  }
  const Document document = ods(rows);

  const Recalculation result = document.recalculate();
  EXPECT_TRUE(result.changed().empty());
  EXPECT_EQ(result.circular().size() + result.unevaluated().size(), 200);
}

TEST(SheetRecalculation, a_structural_edit_makes_every_formula_stale) {
  const Document document =
      ods(row(number("1") + computed("of:=[.A1]*2", "7")));

  first_sheet(document).insert_rows(0, 1);
  const Recalculation result = document.recalculate();

  EXPECT_EQ(spelled(result.changed()), (std::vector<std::string>{"0!B2"}));
  EXPECT_EQ(value_at(document, 1, 1).number(), 2);
}

TEST(SheetRecalculation, a_save_recalculates_an_edit) {
  const Document document =
      ods(row(number("1") + computed("of:=[.A1]*2", "2")));
  first_sheet(document).set_cell(0, 0, CellValue(21));

  const Document saved = reloaded(document);

  EXPECT_EQ(value_at(saved, 1, 0).number(), 42);
}

TEST(SheetRecalculation, an_xlsx_result_replaces_the_stale_one) {
  const Document document = decode(test::ooxml::workbook(
      R"(<row r="1"><c r="A1"><v>2</v></c><c r="B1"><f>A1*2</f><v>4</v></c>)"
      R"(<c r="C1" t="str"><f>"x"&amp;A1</f><v>x2</v></c></row>)"));

  first_sheet(document).set_cell(0, 0, CellValue(3));
  // the xlsx keeps the stale result until a recalculation
  EXPECT_EQ(value_at(document, 1, 0).number(), 4);
  const Recalculation result = document.recalculate();

  EXPECT_EQ(spelled(result.changed()),
            (std::vector<std::string>{"0!B1", "0!C1"}));
  EXPECT_EQ(value_at(document, 1, 0).number(), 6);
  EXPECT_EQ(value_at(document, 2, 0).text(), "x3");
  EXPECT_EQ(value_at(document, 1, 0).formula(), "A1*2");

  const Document saved = reloaded(document);
  EXPECT_EQ(value_at(saved, 1, 0).number(), 6);
  EXPECT_EQ(value_at(saved, 2, 0).text(), "x3");
}

TEST(SheetRecalculation, an_array_formula_gets_no_result) {
  const Document document = decode(test::ooxml::workbook(
      R"(<row r="1"><c r="A1"><v>1</v></c><c r="B1"><v>3</v></c>)"
      R"(<c r="C1"><f t="array" ref="C1">SUM(A1:A2*B1:B2)</f><v>11</v></c>)"
      R"(<c r="D1"><f>C1+1</f><v>12</v></c></row>)"
      R"(<row r="2"><c r="A2"><v>2</v></c><c r="B2"><v>4</v></c></row>)"));

  first_sheet(document).set_cell(0, 0, CellValue(2));
  const Recalculation result = document.recalculate();

  // computed as a plain formula, it reads A1*B1: 6
  EXPECT_TRUE(result.changed().empty());
  EXPECT_EQ(spelled(result.unevaluated()),
            (std::vector<std::string>{"0!C1", "0!D1"}));
  EXPECT_EQ(value_at(document, 2, 0).number(), 11);
}

TEST(SheetRecalculation, a_cell_an_array_formula_spans_is_stale_with_it) {
  const Document document = ods(
      row(number("1") +
          R"(<table:table-cell table:formula="of:=[.A1:.A2]*2")"
          R"( table:number-matrix-columns-spanned="1")"
          R"( table:number-matrix-rows-spanned="2" office:value-type="float")"
          R"( office:value="2"><text:p>2</text:p></table:table-cell>)") +
      row(number("2") +
          R"(<table:covered-table-cell office:value-type="float")"
          R"( office:value="4"><text:p>4</text:p></table:covered-table-cell>)" +
          computed("of:=[.B2]+1", "5")));

  first_sheet(document).set_cell(0, 1, CellValue(10));
  const Recalculation result = document.recalculate();

  EXPECT_TRUE(result.changed().empty());
  EXPECT_EQ(spelled(result.unevaluated()),
            (std::vector<std::string>{"0!B1", "0!C2"}));
}

TEST(SheetRecalculation, an_unedited_document_keeps_its_results) {
  const Document document =
      ods(row(number("1") + computed("of:=[.A1]*2", "2")));

  EXPECT_TRUE(document.recalculate().changed().empty());
}
