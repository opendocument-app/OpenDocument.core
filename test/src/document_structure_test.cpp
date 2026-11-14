#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string>
#include <tuple>
#include <vector>

using namespace odr;

namespace {

std::vector<std::string> sheet_names(const Document &document) {
  std::vector<std::string> result;
  for (const Element child : document.root_element().children()) {
    result.push_back(child.as_sheet().name());
  }
  return result;
}

Document reopen(const Document &document) {
  return open(document.save_to_memory(),
              DecodeOptions::as(document.file_type()))
      .as_document_file()
      .document();
}

class DocumentInsertSheet : public testing::TestWithParam<FileType> {};

} // namespace

INSTANTIATE_TEST_SUITE_P(DocumentStructure, DocumentInsertSheet,
                         testing::Values(FileType::opendocument_spreadsheet,
                                         FileType::office_open_xml_workbook),
                         [](const testing::TestParamInfo<FileType> &info) {
                           return file_type_to_string(info.param);
                         });

TEST_P(DocumentInsertSheet, appends_and_prepends_in_order) {
  const Document document = create_document(GetParam());

  const Sheet last = document.insert_sheet(1, "Last");
  const Sheet first = document.insert_sheet(0, "First");

  EXPECT_EQ(last.name(), "Last");
  EXPECT_EQ(first.name(), "First");
  EXPECT_EQ(sheet_names(document),
            (std::vector<std::string>{"First", "Sheet1", "Last"}));
}

TEST_P(DocumentInsertSheet, an_empty_name_takes_the_first_free_one) {
  const Document document = create_document(GetParam());

  EXPECT_EQ(document.insert_sheet(1, "").name(), "Sheet2");
  std::ignore = document.insert_sheet(2, "Sheet4");
  EXPECT_EQ(document.insert_sheet(3, "").name(), "Sheet5");
}

TEST_P(DocumentInsertSheet, refuses_a_taken_or_illegal_name) {
  const Document document = create_document(GetParam());

  EXPECT_THROW(std::ignore = document.insert_sheet(1, "sheet1"),
               std::invalid_argument);
  EXPECT_THROW(std::ignore = document.insert_sheet(1, "a/b"),
               std::invalid_argument);
  EXPECT_THROW(std::ignore = document.insert_sheet(1, "'quoted'"),
               std::invalid_argument);
  EXPECT_THROW(std::ignore = document.insert_sheet(1, std::string(32, 'x')),
               std::invalid_argument);
  EXPECT_THROW(std::ignore = document.insert_sheet(2, "Far"),
               std::out_of_range);
  EXPECT_EQ(sheet_names(document), std::vector<std::string>{"Sheet1"});
}

TEST_P(DocumentInsertSheet, a_new_sheet_takes_cells_and_survives_a_save) {
  const Document document = create_document(GetParam());

  const Sheet added = document.insert_sheet(0, "Data");
  added.set_cell(1, 2, CellValue("hello"));
  added.set_cell(0, 0, CellValue(42.0));

  const Document saved = reopen(document);
  EXPECT_EQ(sheet_names(saved), (std::vector<std::string>{"Data", "Sheet1"}));
  const Sheet data = (*saved.root_element().children().begin()).as_sheet();
  EXPECT_EQ(data.cell(1, 2).value().text(), "hello");
  EXPECT_EQ(data.cell(0, 0).value().number(), 42.0);
}

TEST_P(DocumentInsertSheet, a_formula_reads_a_sheet_inserted_after_it) {
  const Document document = create_document(GetParam());
  const Sheet first = (*document.root_element().children().begin()).as_sheet();

  const Sheet data = document.insert_sheet(1, "Data");
  data.set_cell(0, 0, CellValue(7.0));
  first.set_cell(
      0, 0,
      CellValue().with_formula(GetParam() == FileType::opendocument_spreadsheet
                                   ? "of:=[$Data.A1]*2"
                                   : "Data!A1*2"));
  std::ignore = document.recalculate();

  EXPECT_EQ(first.cell(0, 0).value().number(), 14.0);
}

TEST(DocumentStructure, a_text_document_refuses_a_sheet) {
  const Document document = create_document(FileType::opendocument_text);
  EXPECT_THROW(std::ignore = document.insert_sheet(0, "Sheet1"),
               UnsupportedOperation);
}
