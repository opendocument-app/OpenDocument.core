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

/// A flat spreadsheet of @p sheets, each written as a `table:table`.
std::string flat_document(const std::string &sheets) {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.spreadsheet">)"
         R"(<office:body><office:spreadsheet>)" +
         sheets + R"(</office:spreadsheet></office:body></office:document>)";
}

std::string sheet(const std::string &name, const std::string &rows) {
  return R"(<table:table table:name=")" + name + R"(">)" + rows +
         R"(</table:table>)";
}

std::string row(const std::string &cells) {
  return R"(<table:table-row>)" + cells + R"(</table:table-row>)";
}

/// A cell computing @p formula, whose cached result nothing here reads.
std::string computed(const std::string &formula) {
  return R"(<table:table-cell table:formula=")" + formula +
         R"(" office:value-type="float" office:value="0">)"
         R"(<text:p>0</text:p></table:table-cell>)";
}

std::string number(const std::string &text) {
  return R"(<table:table-cell office:value-type="float" office:value=")" +
         text + R"("><text:p>)" + text + R"(</text:p></table:table-cell>)";
}

Document decode(const std::string &xml) {
  return DecodedFile(open_strategy::open_file(std::make_shared<MemoryFile>(xml),
                                              {}, Logger::null()))
      .as_document_file()
      .document();
}

std::vector<std::string> spelled(const std::vector<SheetPosition> &positions) {
  std::vector<std::string> result;
  result.reserve(positions.size());
  for (const SheetPosition &position : positions) {
    result.push_back(position.to_string());
  }
  return result;
}

} // namespace

TEST(SheetDependencies, a_formula_reading_a_cell_depends_on_it) {
  const Document document = decode(
      flat_document(sheet("s", row(number("1") + computed("of:=[.A1]*2")))));

  EXPECT_EQ(spelled(document.dependents(SheetPosition(0, 0, 0))),
            (std::vector<std::string>{"0!B1"}));
  EXPECT_TRUE(document.dependents(SheetPosition(0, 1, 0)).empty());
}

TEST(SheetDependencies, a_dependent_of_a_dependent_is_named_too) {
  const Document document = decode(flat_document(sheet(
      "s", row(number("1") + computed("of:=[.A1]") + computed("of:=[.B1]")))));

  EXPECT_EQ(spelled(document.dependents(SheetPosition(0, 0, 0))),
            (std::vector<std::string>{"0!B1", "0!C1"}));
}

TEST(SheetDependencies, a_range_covers_every_position_inside_it) {
  const Document document = decode(
      flat_document(sheet("s", row(number("1") + number("2") + number("3") +
                                   computed("of:=SUM([.A1:.C1])")))));

  EXPECT_EQ(spelled(document.dependents(SheetPosition(0, 1, 0))),
            (std::vector<std::string>{"0!D1"}));
  EXPECT_TRUE(document.dependents(SheetPosition(0, 4, 0)).empty());
}

TEST(SheetDependencies, a_formula_on_another_sheet_names_the_sheet_it_reads) {
  const Document document =
      decode(flat_document(sheet("Data", row(number("1"))) +
                           sheet("Report", row(computed("of:=[Data.A1]*2")))));

  EXPECT_EQ(spelled(document.dependents(SheetPosition(0, 0, 0))),
            (std::vector<std::string>{"1!A1"}));
  EXPECT_TRUE(document.dependents(SheetPosition(1, 0, 0)).empty());
}

TEST(SheetDependencies, a_sheet_name_is_matched_without_case) {
  const Document document =
      decode(flat_document(sheet("Data", row(number("1"))) +
                           sheet("Report", row(computed("of:=[DATA.A1]")))));

  EXPECT_EQ(spelled(document.dependents(SheetPosition(0, 0, 0))),
            (std::vector<std::string>{"1!A1"}));
}

TEST(SheetDependencies, a_batch_names_each_dependent_once) {
  const Document document = decode(flat_document(sheet(
      "s", row(number("1") + number("2") + computed("of:=[.A1]+[.B1]")))));

  EXPECT_EQ(spelled(document.dependents(
                {SheetPosition(0, 0, 0), SheetPosition(0, 1, 0)})),
            (std::vector<std::string>{"0!C1"}));
}

TEST(SheetDependencies, a_formula_naming_no_position_reads_nothing) {
  const Document document =
      decode(flat_document(sheet("s", row(computed("of:=TODAY()")))));

  EXPECT_TRUE(document.dependents(SheetPosition(0, 0, 0)).empty());
  EXPECT_TRUE(document.unresolved_formulas().empty());
}

TEST(SheetDependencies, a_formula_naming_a_named_expression_is_unresolved) {
  const Document document =
      decode(flat_document(sheet("s", row(computed("of:=SUM($$Sales)")))));

  EXPECT_EQ(spelled(document.unresolved_formulas()),
            (std::vector<std::string>{"0!A1"}));
}

TEST(SheetDependencies, a_formula_that_does_not_parse_is_unresolved) {
  const Document document =
      decode(flat_document(sheet("s", row(computed("of:=[.A1] +")))));

  EXPECT_EQ(spelled(document.unresolved_formulas()),
            (std::vector<std::string>{"0!A1"}));
}

/// An edit here reaches no other file, so the cell is not unresolved either.
TEST(SheetDependencies, a_reference_into_another_document_is_left_alone) {
  const Document document = decode(flat_document(
      sheet("s", row(computed("of:=['file:///x.ods'#$Sheet1.A1]")))));

  EXPECT_TRUE(document.unresolved_formulas().empty());
  EXPECT_TRUE(document.dependents(SheetPosition(0, 0, 0)).empty());
}

TEST(SheetDependencies, a_document_holding_no_sheet_answers_nothing) {
  const Document document =
      decode(R"(<?xml version="1.0" encoding="UTF-8"?>)"
             R"(<office:document office:mimetype=")"
             R"(application/vnd.oasis.opendocument.text">)"
             R"(<office:body><office:text><text:p>x</text:p>)"
             R"(</office:text></office:body></office:document>)");

  EXPECT_TRUE(document.dependents(SheetPosition(0, 0, 0)).empty());
  EXPECT_TRUE(document.unresolved_formulas().empty());
}

TEST(SheetDependencies, an_xlsx_formula_is_read_in_its_own_syntax) {
  const Document document = odr::test::ooxml::decode(odr::test::ooxml::workbook(
      R"(<row r="1"><c r="A1"><v>1</v></c>)"
      R"(<c r="B1"><f>SUM(A1:A3)</f><v>1</v></c></row>)"));

  EXPECT_EQ(spelled(document.dependents(SheetPosition(0, 0, 2))),
            (std::vector<std::string>{"0!B1"}));
}

/// `A:A` is every row of the column, so a cell far down it is still read.
TEST(SheetDependencies, a_whole_column_is_read_to_its_end) {
  const Document document = odr::test::ooxml::decode(odr::test::ooxml::workbook(
      R"(<row r="1"><c r="B1"><f>SUM(A:A)</f><v>1</v></c></row>)"));

  EXPECT_EQ(spelled(document.dependents(SheetPosition(0, 0, 999))),
            (std::vector<std::string>{"0!B1"}));
  EXPECT_TRUE(document.dependents(SheetPosition(0, 2, 0)).empty());
}

/// Each member reads the master's expression moved, so its own row, not A1.
TEST(SheetDependencies, every_member_of_a_shared_group_reads_its_own_row) {
  const Document document = odr::test::ooxml::decode(odr::test::ooxml::workbook(
      R"(<row r="1"><c r="A1"><v>1</v></c>)"
      R"(<c r="C1"><f t="shared" ref="C1:C2" si="0">A1</f><v>1</v></c></row>)"
      R"(<row r="2"><c r="A2"><v>2</v></c>)"
      R"(<c r="C2"><f t="shared" si="0"/><v>2</v></c></row>)"));

  EXPECT_EQ(spelled(document.dependents(SheetPosition(0, 0, 1))),
            (std::vector<std::string>{"0!C2"}));
}
