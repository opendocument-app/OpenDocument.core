#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/sheet_cell_source.hpp>
#include <odr/internal/formula/formula_value.hpp>
#include <odr/internal/open_strategy.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace odr;
using namespace odr::internal;
using formula::ErrorType;
using formula::Value;

namespace {

std::shared_ptr<abstract::Document>
decode(const std::shared_ptr<abstract::File> &file) {
  const std::shared_ptr<abstract::DecodedFile> decoded =
      open_strategy::open_file(file, {}, Logger::null());
  return std::dynamic_pointer_cast<abstract::DocumentFile>(decoded)->document();
}

/// A flat spreadsheet of one sheet `s` stating @p rows, with @p settings in
/// front of it.
std::shared_ptr<abstract::Document> ods(const std::string &rows,
                                        const std::string &settings = "") {
  return decode(std::make_shared<MemoryFile>(
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.spreadsheet">)"
      R"(<office:body><office:spreadsheet>)" +
      settings + R"(<table:table table:name="s">)" + rows +
      R"(</table:table></office:spreadsheet></office:body></office:document>)"));
}

std::string row(const std::string &cells) {
  return "<table:table-row>" + cells + "</table:table-row>";
}

std::optional<Value> at(const SheetCellSource &source,
                        const std::uint32_t column, const std::uint32_t row) {
  return source.cell(SheetPosition(0, column, row));
}

} // namespace

TEST(SheetCellSource, an_ods_cell_reads_as_the_evaluator_reads_it) {
  const std::shared_ptr<abstract::Document> document = ods(row(
      R"(<table:table-cell office:value-type="float" office:value="2.5">)"
      R"(<text:p>2.5</text:p></table:table-cell>)"
      R"(<table:table-cell/>)"
      R"(<table:table-cell office:value-type="string"><text:p/></table:table-cell>)"
      R"(<table:table-cell table:formula="of:=1/0" office:value-type="string")"
      R"( office:string-value="" calcext:value-type="error">)"
      R"(<text:p>#DIV/0!</text:p></table:table-cell>)"
      R"(<table:table-cell table:formula="of:=1+1"/>)"
      R"(<table:table-cell office:value-type="boolean")"
      R"( office:boolean-value="true"><text:p>TRUE</text:p></table:table-cell>)"));
  const SheetCellSource source(*document);

  EXPECT_EQ(at(source, 0, 0), Value{2.5});
  EXPECT_EQ(at(source, 1, 0), Value{formula::Empty{}});
  EXPECT_EQ(at(source, 2, 0), Value{std::string()});
  EXPECT_EQ(at(source, 3, 0), Value{ErrorType::division});
  // a formula that caches no result has no answer
  EXPECT_EQ(at(source, 4, 0), std::nullopt);
  EXPECT_EQ(at(source, 5, 0), Value{true});
  EXPECT_EQ(source.extent(0).columns, 6);
}

TEST(SheetCellSource, an_ods_date_is_counted_from_its_null_date) {
  const std::string date =
      R"(<table:table-cell office:value-type="date")"
      R"( office:date-value="1904-01-02"><text:p>1904-01-02</text:p>)"
      R"(</table:table-cell>)";

  const std::shared_ptr<abstract::Document> plain = ods(row(date));
  EXPECT_EQ(at(SheetCellSource(*plain), 0, 0), Value{1463.0});

  const std::shared_ptr<abstract::Document> shifted = ods(
      row(date), R"(<table:calculation-settings table:case-sensitive="false">)"
                 R"(<table:null-date table:date-value="1904-01-01"/>)"
                 R"(</table:calculation-settings>)");
  const SheetCellSource source(*shifted);
  EXPECT_EQ(at(source, 0, 0), Value{1.0});
  EXPECT_FALSE(source.settings().case_sensitive);
  EXPECT_EQ(source.settings().dialect, formula::Dialect::libreoffice);
}

TEST(SheetCellSource, an_xlsx_cell_reads_as_the_evaluator_reads_it) {
  const std::shared_ptr<abstract::Document> document =
      decode(test::ooxml::workbook(
          R"(<row r="1"><c r="A1"><v>2.5</v></c><c r="B1" s="0"/>)"
          R"(<c r="C1" t="str"><f>""</f><v></v></c>)"
          R"(<c r="D1" t="e"><f>1/0</f><v>#DIV/0!</v></c>)"
          R"(<c r="E1"><f>1+1</f></c><c r="F1" t="b"><v>1</v></c></row>)"));
  const SheetCellSource source(*document);

  EXPECT_EQ(at(source, 0, 0), Value{2.5});
  EXPECT_EQ(at(source, 1, 0), Value{formula::Empty{}});
  EXPECT_EQ(at(source, 2, 0), Value{std::string()});
  EXPECT_EQ(at(source, 3, 0), Value{ErrorType::division});
  EXPECT_EQ(at(source, 4, 0), std::nullopt);
  EXPECT_EQ(at(source, 5, 0), Value{true});
  EXPECT_EQ(source.sheet("S"), 0);
  EXPECT_EQ(source.settings().dialect, formula::Dialect::excel);
}

TEST(SheetCellSource, an_old_ods_error_is_a_number_showing_its_spelling) {
  const std::shared_ptr<abstract::Document> document = ods(row(
      R"x(<table:table-cell table:formula="oooc:=NA()" office:value-type="float")x"
      R"( office:value="0"><text:p>#N/A</text:p></table:table-cell>)"
      R"(<table:table-cell table:formula="oooc:=1/0" office:value-type="float")"
      R"( office:value="0"><text:p>Err:503</text:p></table:table-cell>)"
      R"(<table:table-cell table:formula="of:=&quot;#N/A&quot;")"
      R"( office:value-type="string" office:string-value="#N/A">)"
      R"(<text:p>#N/A</text:p></table:table-cell>)"));
  const SheetCellSource source(*document);

  EXPECT_EQ(at(source, 0, 0), Value{ErrorType::not_available});
  EXPECT_EQ(at(source, 1, 0), std::nullopt);
  EXPECT_EQ(at(source, 2, 0), Value{std::string("#N/A")});
}

TEST(SheetCellSource, a_position_a_merge_covers_is_empty) {
  const std::shared_ptr<abstract::Document> document = ods(
      row(R"(<table:table-cell table:number-columns-spanned="2")"
          R"( office:value-type="string"><text:p>m</text:p></table:table-cell>)"
          R"(<table:covered-table-cell/>)"
          R"(<table:table-cell office:value-type="float" office:value="3">)"
          R"(<text:p>3</text:p></table:table-cell>)"));
  const SheetCellSource source(*document);

  EXPECT_EQ(at(source, 0, 0), Value{std::string("m")});
  EXPECT_EQ(at(source, 1, 0), Value{formula::Empty{}});
  EXPECT_EQ(at(source, 2, 0), Value{3.0});
}

TEST(SheetCellSource, an_ods_name_is_local_to_its_sheet_first) {
  const std::shared_ptr<abstract::Document> document = decode(std::make_shared<
                                                              MemoryFile>(
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.spreadsheet">)"
      R"(<office:body><office:spreadsheet>)"
      R"(<table:table table:name="s"><table:table-row><table:table-cell/>)"
      R"(</table:table-row><table:named-expressions>)"
      R"(<table:named-expression table:name="Rate" table:expression="of:=2"/>)"
      R"(</table:named-expressions></table:table>)"
      R"(<table:table table:name="t"><table:table-row><table:table-cell/>)"
      R"(</table:table-row></table:table>)"
      R"(<table:named-expressions>)"
      R"(<table:named-range table:name="Total")"
      R"( table:cell-range-address="$s.$A$1:.$A$3"/>)"
      R"(<table:named-expression table:name="rate" table:expression="of:=3"/>)"
      R"(</table:named-expressions>)"
      R"(</office:spreadsheet></office:body></office:document>)"));
  const SheetCellSource source(*document);

  const std::optional<formula::Node> total = source.name("TOTAL", 1);
  ASSERT_TRUE(total.has_value());
  EXPECT_TRUE(total->holds<formula::RangeReference>());
  const std::optional<formula::Node> local = source.name("rate", 0);
  ASSERT_TRUE(local.has_value());
  EXPECT_EQ(local->get<formula::NumberLiteral>().value, 2);
  const std::optional<formula::Node> global = source.name("rate", 1);
  ASSERT_TRUE(global.has_value());
  EXPECT_EQ(global->get<formula::NumberLiteral>().value, 3);
  EXPECT_FALSE(source.name("nope", 0).has_value());
}

TEST(SheetCellSource, the_first_of_two_names_of_one_spelling_counts) {
  const std::shared_ptr<abstract::Document> document =
      decode(test::ooxml::workbook(
          R"(<row r="1"><c r="A1"><v>2</v></c></row>)", "", "",
          R"(<definedNames><definedName name="Rate">1</definedName>)"
          R"(<definedName name="RATE">2</definedName></definedNames>)"));
  const SheetCellSource source(*document);

  const std::optional<formula::Node> rate = source.name("rate", 0);
  ASSERT_TRUE(rate.has_value());
  EXPECT_EQ(rate->get<formula::NumberLiteral>().value, 1);
}

TEST(SheetCellSource, an_xlsx_name_states_its_sheet_by_index) {
  const std::shared_ptr<abstract::Document> document =
      decode(test::ooxml::workbook(
          R"(<row r="1"><c r="A1"><v>2</v></c></row>)", "", "",
          R"(<definedNames><definedName name="Total">s!$A$1</definedName>)"
          R"(<definedName name="Rate" localSheetId="0">0.5</definedName>)"
          R"(</definedNames>)"));
  const SheetCellSource source(*document);

  const std::optional<formula::Node> total = source.name("total", 0);
  ASSERT_TRUE(total.has_value());
  EXPECT_TRUE(total->holds<formula::CellReference>());
  const std::optional<formula::Node> rate = source.name("RATE", 0);
  ASSERT_TRUE(rate.has_value());
  EXPECT_EQ(rate->get<formula::NumberLiteral>().value, 0.5);
}

TEST(SheetCellSource, the_generator_of_an_ods_decides_a_boolean_in_a_join) {
  const auto boolean_word = [](const std::string &meta) {
    const std::shared_ptr<abstract::Document> document =
        decode(std::make_shared<MemoryFile>(
            R"(<?xml version="1.0" encoding="UTF-8"?>)"
            R"(<office:document office:mimetype=")"
            R"(application/vnd.oasis.opendocument.spreadsheet">)" +
            meta +
            R"(<office:body><office:spreadsheet><table:table table:name="s">)"
            R"(</table:table></office:spreadsheet></office:body>)"
            R"(</office:document>)"));
    return SheetCellSource(*document).settings().boolean_word;
  };
  const auto generator = [](const std::string &name) {
    return "<office:meta><meta:generator>" + name +
           "</meta:generator></office:meta>";
  };
  EXPECT_EQ(boolean_word(generator("LibreOffice/26.8.1.2$Linux_X86_64")),
            false);
  EXPECT_EQ(boolean_word(generator("LibreOffice/27.2.0.3$Linux_X86_64")), true);
  EXPECT_EQ(boolean_word(generator("LibreOfficeDev/27.2.0.0.alpha0$Linux")),
            std::nullopt);
  EXPECT_EQ(boolean_word(generator("MicrosoftOffice/16.00$Windows_X86_64")),
            std::nullopt);
  EXPECT_EQ(boolean_word(""), std::nullopt);
}

TEST(SheetCellSource, a_range_reads_what_its_cells_read) {
  const std::shared_ptr<abstract::Document> document = ods(
      row(R"(<table:table-cell table:number-columns-repeated="3")"
          R"( office:value-type="float" office:value="2">)"
          R"(<text:p>2</text:p></table:table-cell><table:table-cell/>)"
          R"(<table:table-cell office:value-type="string"><text:p>x</text:p>)"
          R"(</table:table-cell>)") +
      R"(<table:table-row table:number-rows-repeated="2">)"
      R"(<table:table-cell/><table:table-cell office:value-type="float")"
      R"( office:value="5"><text:p>5</text:p></table:table-cell>)"
      R"(</table:table-row>)");
  const SheetCellSource source(*document);

  std::vector<std::string> visited;
  source.for_each_cell(
      formula::Area{0, TableRange({0, 0}, {4, 2})},
      [&](const SheetPosition &position, const std::optional<Value> &value) {
        ASSERT_TRUE(value.has_value());
        EXPECT_EQ(source.cell(position), value) << position.to_string();
        visited.push_back(position.to_string());
      });
  EXPECT_EQ(visited, (std::vector<std::string>{"0!A1", "0!B1", "0!C1", "0!E1",
                                               "0!B2", "0!B3"}));
}
