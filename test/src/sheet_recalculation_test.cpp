#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/sheet_position.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/open_strategy.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <memory>
#include <sstream>
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

// The clock and the random numbers are the machine's, so the test reads only
// what they cannot change.
TEST(SheetRecalculation, now_and_rand_compute_every_time) {
  const Document document =
      ods(row(computed("of:=TODAY()", "1") + computed("of:=NOW()", "1") +
              computed("of:=RAND()", "5")));
  document.recalculate();

  const double today = value_at(document, 0, 0).number();
  EXPECT_GT(today, 45000); // after 2023
  EXPECT_EQ(today, std::floor(today));
  EXPECT_GE(value_at(document, 1, 0).number(), today);
  EXPECT_LT(value_at(document, 1, 0).number(), today + 1);
  EXPECT_GE(value_at(document, 2, 0).number(), 0);
  EXPECT_LT(value_at(document, 2, 0).number(), 1);
}

TEST(SheetRecalculation, a_range_reads_the_stale_cells_it_holds) {
  const Document document =
      ods(row(number("1")) + row(computed("of:=[.A1]*2", "2")) +
          row(computed("of:=SUM([.A1:.A2])", "3")));

  first_sheet(document).set_cell(0, 0, CellValue(5));
  document.recalculate();

  EXPECT_EQ(value_at(document, 0, 2).number(), 15);
}

TEST(SheetRecalculation, a_running_total_down_a_column) {
  std::string rows = row(number("1"));
  for (std::uint32_t i = 2; i <= 2000; ++i) {
    rows += row(uncomputed("of:=[.A" + std::to_string(i - 1) + "]+1"));
  }
  const Document document = ods(rows);

  EXPECT_EQ(document.recalculate().changed().size(), 1999);
  EXPECT_EQ(value_at(document, 0, 1999).number(), 2000);
}

TEST(SheetRecalculation, a_chain_up_a_column_past_the_depth_limit) {
  std::string rows;
  for (std::uint32_t i = 1; i < 1100; ++i) {
    rows += row(uncomputed("of:=[.A" + std::to_string(i + 1) + "]+1"));
  }
  rows += row(number("1"));
  const Document document = ods(rows);

  EXPECT_EQ(document.recalculate().changed().size(), 1099);
  EXPECT_EQ(value_at(document, 0, 0).number(), 1100);
}

TEST(SheetRecalculation, a_cycle_longer_than_the_depth_limit_gets_no_result) {
  std::string rows = row(uncomputed("of:=[.A2]+1"));
  for (std::uint32_t i = 2; i <= 201; ++i) {
    rows += row(
        uncomputed("of:=[.A" + std::to_string(i == 201 ? 2 : i + 1) + "]+1"));
  }
  const Document document = ods(rows);

  const Recalculation result = document.recalculate();
  EXPECT_TRUE(result.changed().empty());
  EXPECT_EQ(result.circular().size(), 200);
  EXPECT_EQ(spelled(result.unevaluated()), (std::vector<std::string>{"0!A1"}));
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

TEST(SheetRecalculation, an_array_formula_reads_every_cell_of_its_ranges) {
  const Document document = decode(test::ooxml::workbook(
      R"(<row r="1"><c r="A1"><v>1</v></c><c r="B1"><v>3</v></c>)"
      R"(<c r="C1"><f t="array" ref="C1">SUM(A1:A2*B1:B2)</f><v>11</v></c>)"
      R"(<c r="D1"><f>C1+1</f><v>12</v></c></row>)"
      R"(<row r="2"><c r="A2"><v>2</v></c><c r="B2"><v>4</v></c></row>)"));

  first_sheet(document).set_cell(0, 0, CellValue(2));
  const Recalculation result = document.recalculate();

  // computed as a plain formula, it would read A1*B1: 6
  EXPECT_EQ(spelled(result.changed()),
            (std::vector<std::string>{"0!C1", "0!D1"}));
  EXPECT_EQ(value_at(document, 2, 0).number(), 14);
  EXPECT_EQ(value_at(document, 3, 0).number(), 15);
}

TEST(SheetRecalculation, an_array_formula_fills_its_range) {
  const Document document = decode(test::ooxml::workbook(
      R"(<row r="1"><c r="A1"><v>1</v></c>)"
      R"(<c r="B1"><f t="array" ref="B1:B3">A1:A2*2</f><v>2</v></c></row>)"
      R"(<row r="2"><c r="A2"><v>2</v></c><c r="B2"><v>4</v></c></row>)"
      R"(<row r="3"><c r="B3"><v>0</v></c></row>)"));

  first_sheet(document).set_cell(0, 1, CellValue(5));
  document.recalculate();

  EXPECT_EQ(value_at(document, 1, 0).number(), 2);
  EXPECT_EQ(value_at(document, 1, 1).number(), 10);
  // past the result
  EXPECT_EQ(value_at(document, 1, 2).text(), "#N/A");
  const Document saved = reloaded(document);
  EXPECT_EQ(value_at(saved, 1, 1).number(), 10);
}

TEST(SheetRecalculation, a_cell_an_array_formula_spans_computes_with_it) {
  const Document document = ods(
      row(number("1") +
          R"(<table:table-cell table:formula="of:=[.A1:.A2]*2")"
          R"( table:number-matrix-columns-spanned="1")"
          R"( table:number-matrix-rows-spanned="2" office:value-type="float")"
          R"( office:value="2"><text:p>2</text:p></table:table-cell>)") +
      // a cell of the range as LibreOffice writes one
      row(number("2") + number("4") + computed("of:=[.B2]+1", "5")));

  first_sheet(document).set_cell(0, 1, CellValue(10));
  const Recalculation result = document.recalculate();

  // the write dropped the result of B1, as an ods does for each dependent
  EXPECT_EQ(spelled(result.changed()),
            (std::vector<std::string>{"0!B1", "0!B2", "0!C2"}));
  EXPECT_EQ(value_at(document, 1, 1).number(), 20);
  EXPECT_EQ(value_at(document, 2, 1).number(), 21);
  const Document saved = reloaded(document);
  EXPECT_EQ(value_at(saved, 2, 1).number(), 21);
}

// ECMA-376 18.14: the copy of another workbook that Excel caches, as it reads
// one while the workbook is closed.
TEST(SheetRecalculation, a_reference_into_another_workbook_reads_its_copy) {
  internal::zip::ZipArchive zip;
  const std::string main =
      R"(xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
      R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships")";
  const std::string relationships =
      R"(xmlns="http://schemas.openxmlformats.org/package/2006/relationships")";
  test::ooxml::insert(
      zip, "_rels/.rels",
      "<Relationships " + relationships +
          R"(><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/></Relationships>)");
  test::ooxml::insert(
      zip, "xl/workbook.xml",
      "<workbook " + main +
          R"(><sheets><sheet name="s" sheetId="1" r:id="rId1"/></sheets>)"
          R"(<externalReferences><externalReference r:id="rId2"/>)"
          R"(</externalReferences></workbook>)");
  test::ooxml::insert(
      zip, "xl/_rels/workbook.xml.rels",
      "<Relationships " + relationships +
          R"(><Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
          R"(<Relationship Id="rId2" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/externalLink" Target="externalLinks/externalLink1.xml"/></Relationships>)");
  test::ooxml::insert(
      zip, "xl/externalLinks/externalLink1.xml",
      "<externalLink " + main +
          R"(><externalBook r:id="rId1"><sheetNames><sheetName val="Data"/>)"
          R"(<sheetName val="Dates"/></sheetNames><sheetDataSet>)"
          R"(<sheetData sheetId="0"><row r="1"><cell r="A1"><v>5</v></cell>)"
          R"(<cell r="B1"><v>7</v></cell><cell r="C1"/></row>)"
          R"(<row r="2"><cell r="A2" t="str"><v>x</v></cell>)"
          R"(<cell r="B2"><v>11</v></cell></row></sheetData>)"
          R"(<sheetData sheetId="1"><row r="1"><cell r="A1" t="d">)"
          R"(<v>2024-01-01</v></cell></row></sheetData>)"
          R"(</sheetDataSet></externalBook></externalLink>)");
  test::ooxml::insert(
      zip, "xl/worksheets/sheet1.xml",
      "<worksheet " + main +
          R"(><sheetData><row r="1"><c r="A1"><f>[1]Data!A1*2</f></c>)"
          R"(<c r="B1"><f>SUM([1]Data!A1:B2)</f></c>)"
          R"(<c r="C1"><f>[1]data!A2</f></c>)"
          R"(<c r="D1"><f>[2]Data!A1</f></c>)"
          R"(<c r="E1"><f>[1]Other!A1</f></c>)"
          R"(<c r="F1"><f>[1]Data!C1&amp;"y"</f></c>)"
          R"(<c r="G1"><f>[1]Dates!A1</f></c></row></sheetData></worksheet>)");
  std::stringstream out;
  zip.save(out);
  const Document document =
      decode(std::make_shared<internal::MemoryFile>(out.str()));

  const Recalculation result = document.recalculate();

  EXPECT_EQ(value_at(document, 0, 0).number(), 10);
  EXPECT_EQ(value_at(document, 1, 0).number(), 23);
  EXPECT_EQ(value_at(document, 2, 0).text(), "x");
  // a cell the copy states without a value is empty
  EXPECT_EQ(value_at(document, 5, 0).text(), "y");
  // a book the file names nothing for, a sheet its copy lacks, and a copy
  // with a cell of a type this does not read
  EXPECT_EQ(spelled(result.unevaluated()),
            (std::vector<std::string>{"0!D1", "0!E1", "0!G1"}));
}

namespace {

/// The cycles of the LibreOffice probe in a flat spreadsheet that computes a
/// cycle in at most @p steps: A1 and B1 read each other, C1 reads A1, A2 grows
/// without end, B2 reads itself and A3, and B3 divides by itself.
Document iterating_ods(const std::string &steps) {
  return decode(std::make_shared<MemoryFile>(
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.spreadsheet">)"
      R"(<office:body><office:spreadsheet><table:calculation-settings>)"
      R"(<table:iteration table:status="enable" table:steps=")" +
      steps +
      R"(" table:maximum-difference="0.001"/></table:calculation-settings>)"
      R"(<table:table table:name="s">)" +
      row(uncomputed("of:=[.B1]/2+1") + uncomputed("of:=[.A1]") +
          uncomputed("of:=[.A1]*10")) +
      row(uncomputed("of:=[.A2]+1") + uncomputed("of:=[.B2]/2+[.A3]")) +
      row(number("4") + uncomputed("of:=([.B3]+9/[.B3])/2")) +
      R"(</table:table></office:spreadsheet></office:body></office:document>)"));
}

} // namespace

// The results are the ones LibreOffice computed for the same cells: each
// step reads what the step before left, from 0, until no cell changes by the
// difference.
TEST(SheetRecalculation, a_cycle_computes_where_the_document_iterates) {
  const Document document = iterating_ods("100");
  const Recalculation result = document.recalculate();

  EXPECT_EQ(value_at(document, 0, 0).number(), 1.9990234375);
  EXPECT_EQ(value_at(document, 1, 0).number(), 1.9990234375);
  EXPECT_EQ(value_at(document, 2, 0).number(), 19.990234375);
  EXPECT_EQ(value_at(document, 1, 1).number(), 7.9990234375);
  EXPECT_EQ(value_at(document, 1, 2).text(), "#DIV/0!");
  // LibreOffice states `Err:523` for a cycle that does not converge
  EXPECT_EQ(spelled(result.circular()), (std::vector<std::string>{"0!A2"}));
}

TEST(SheetRecalculation, a_cycle_past_its_steps_has_no_result_in_libreoffice) {
  const Document document = iterating_ods("3");
  const Recalculation result = document.recalculate();

  EXPECT_EQ(spelled(result.circular()),
            (std::vector<std::string>{"0!A1", "0!B1", "0!A2", "0!B2"}));
  EXPECT_EQ(value_at(document, 2, 0).type(), ValueType::unknown);
}

TEST(SheetRecalculation,
     a_cycle_past_its_steps_keeps_its_last_values_in_excel) {
  const Document document = decode(test::ooxml::workbook(
      R"(<row r="1"><c r="A1"><f>B1/2+1</f></c><c r="B1"><f>A1</f></c>)"
      R"(<c r="C1"><f>A1*10</f></c></row>)",
      "", "", R"(<calcPr iterate="1" iterateCount="3"/>)"));
  document.recalculate();

  // three steps from 0: 1, 1.5, 1.75
  EXPECT_EQ(value_at(document, 0, 0).number(), 1.75);
  EXPECT_EQ(value_at(document, 2, 0).number(), 17.5);
}

TEST(SheetRecalculation, an_unedited_document_keeps_its_results) {
  const Document document =
      ods(row(number("1") + computed("of:=[.A1]*2", "2")));

  EXPECT_TRUE(document.recalculate().changed().empty());
}

TEST(SheetRecalculation, oversized_array_spans_are_rejected_before_expansion) {
  const Document document =
      ods(row(R"(<table:table-cell table:formula="of:=1" )"
              R"(table:number-matrix-columns-spanned="65536" )"
              R"(table:number-matrix-rows-spanned="65536"/> )"));
  EXPECT_THROW((void)document.recalculate(), UnsupportedOperation);
}
