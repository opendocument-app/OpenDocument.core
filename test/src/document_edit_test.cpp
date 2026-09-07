#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>

#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/open_strategy.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <stdexcept>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

/// One sheet whose single row holds three string cells.
Document three_cell_sheet() {
  const std::string source =
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.spreadsheet">)"
      R"(<office:body><office:spreadsheet><table:table table:name="s">)"
      R"(<table:table-row>)"
      R"(<table:table-cell office:value-type="string"><text:p>a</text:p></table:table-cell>)"
      R"(<table:table-cell office:value-type="string"><text:p>b</text:p></table:table-cell>)"
      R"(<table:table-cell office:value-type="string"><text:p>c</text:p></table:table-cell>)"
      R"(</table:table-row></table:table>)"
      R"(</office:spreadsheet></office:body></office:document>)";
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

TEST(DocumentEdit, the_ops_are_applied_in_order) {
  const Document document = three_cell_sheet();

  document.edit(R"({"version":1,"ops":[)"
                R"({"op":"setCell","sheet":0,"column":0,"row":0,)"
                R"("value":{"type":"string","text":"first"}},)"
                R"({"op":"setCell","sheet":0,"column":0,"row":0,)"
                R"("value":{"type":"string","text":"second"}}]})");

  EXPECT_EQ(first_sheet(document).cell(0, 0).value().text(), "second");
}

TEST(DocumentEdit, a_number_op_states_the_number_and_the_text) {
  const Document document = three_cell_sheet();

  document.edit(R"({"version":1,"ops":[)"
                R"({"op":"setCell","sheet":0,"column":1,"row":0,)"
                R"("value":{"type":"number","number":12.5,"text":"12,50"}}]})");

  const CellValue value = first_sheet(document).cell(1, 0).value();
  EXPECT_EQ(value.type(), ValueType::float_number);
  ASSERT_TRUE(value.has_number());
  EXPECT_DOUBLE_EQ(value.number(), 12.5);
  EXPECT_EQ(value.text(), "12,50");
}

/// The text is optional: the number spells itself.
TEST(DocumentEdit, a_number_op_without_text_spells_itself) {
  const Document document = three_cell_sheet();

  document.edit(R"({"version":1,"ops":[)"
                R"({"op":"setCell","sheet":0,"column":1,"row":0,)"
                R"("value":{"type":"number","number":12.5}}]})");

  EXPECT_EQ(first_sheet(document).cell(1, 0).value().text(), "12.5");
}

TEST(DocumentEdit, an_empty_op_clears_the_cell) {
  const Document document = three_cell_sheet();

  document.edit(R"({"version":1,"ops":[)"
                R"({"op":"setCell","sheet":0,"column":2,"row":0,)"
                R"("value":{"type":"empty"}}]})");

  EXPECT_EQ(first_sheet(document).cell(2, 0).value().text(), "");
}

TEST(DocumentEdit, a_text_op_names_its_element_by_path) {
  const Document document = three_cell_sheet();

  document.edit(
      R"({"version":1,"ops":[{"op":"setText",)"
      R"("path":"/child:0/cell:A1/child:0/child:0","text":"typed"}]})");

  EXPECT_EQ(first_sheet(document).cell(0, 0).value().text(), "typed");
}

TEST(DocumentEdit, an_unknown_version_refuses) {
  const Document document = three_cell_sheet();

  EXPECT_THROW(document.edit(R"({"version":2,"ops":[]})"),
               std::invalid_argument);
  EXPECT_THROW(document.edit(R"({"ops":[]})"), std::invalid_argument);
}

TEST(DocumentEdit, an_unknown_op_refuses) {
  const Document document = three_cell_sheet();

  EXPECT_THROW(document.edit(R"({"version":1,"ops":[{"op":"setStyle"}]})"),
               std::invalid_argument);
}

TEST(DocumentEdit, an_op_naming_a_sheet_that_is_not_there_refuses) {
  const Document document = three_cell_sheet();

  EXPECT_THROW(document.edit(R"({"version":1,"ops":[)"
                             R"({"op":"setCell","sheet":3,"column":0,"row":0,)"
                             R"("value":{"type":"empty"}}]})"),
               std::invalid_argument);
}

/// It throws on the first op it cannot apply, so the ones before it stand.
TEST(DocumentEdit, the_ops_before_a_refusal_are_applied) {
  const Document document = three_cell_sheet();

  EXPECT_ANY_THROW(
      document.edit(R"({"version":1,"ops":[)"
                    R"({"op":"setCell","sheet":0,"column":0,"row":0,)"
                    R"("value":{"type":"string","text":"written"}},)"
                    R"({"op":"setCell","sheet":0,"column":9,"row":9,)"
                    R"("value":{"type":"string","text":"absent"}}]})"));

  EXPECT_EQ(first_sheet(document).cell(0, 0).value().text(), "written");
}
