#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <vector>

using namespace odr;
using namespace odr::test;

// `sheet-grid.md` decision 3: an edit in the grid past the content is the
// `setCell` an edit inside it is, so every engine that writes a sheet must
// take a position past what the file states.

namespace {

Document csv(const File &file) {
  return open(file, DecodeOptions::as(FileType::comma_separated_values))
      .as_csv_file()
      .document();
}

Document reopened(const Document &document) {
  const File saved = document.save_to_memory();
  if (document.file_type() == FileType::comma_separated_values) {
    return csv(saved);
  }
  return open(saved).as_document_file().document();
}

Sheet first_sheet(const Document &document) {
  return document.root_element().first_child().as_sheet();
}

std::string text_at(const Sheet &sheet, const std::uint32_t column,
                    const std::uint32_t row) {
  const CellValue value = sheet.cell(column, row).value();
  return value.has_text() ? value.text() : std::string();
}

void set_cell(const Document &document, const std::uint32_t column,
              const std::uint32_t row, const std::string &value) {
  document.edit(R"({"version":2,"ops":[{"op":"setCell","sheet":0,"column":)" +
                std::to_string(column) + R"(,"row":)" + std::to_string(row) +
                R"(,"value":)" + value + "}]}");
}

struct Case final {
  std::string name;
  std::function<Document()> open;
};

Document open_input(const std::string &path) {
  return open(TestData::test_file_path(path)).as_document_file().document();
}

std::vector<Case> cases() {
  return {
      {"a new ods",
       [] { return create_document(FileType::opendocument_spreadsheet); }},
      {"a new xlsx",
       [] { return create_document(FileType::office_open_xml_workbook); }},
      // the rows past the content are one `table:table-row` repeated to the
      // end of the grid
      {"an ods of ten rows",
       [] { return open_input("odr-public/ods/file_example_ODS_10.ods"); }},
      // past the content is no `<row>` at all, and `<dimension>` names the
      // used range
      {"an xlsx of ten rows",
       [] { return open_input("odr-public/xlsx/file_example_XLSX_10.xlsx"); }},
      {"a csv", [] { return csv(File::from_memory("a,b\nc,d\n")); }},
  };
}

} // namespace

TEST(SheetPastContent, a_cell_below_and_beside_the_content_survives_a_save) {
  for (const Case &test_case : cases()) {
    SCOPED_TRACE(test_case.name);
    const Document document = test_case.open();
    const Sheet sheet = first_sheet(document);
    const TableDimensions content = sheet.content(std::nullopt);
    const std::string first = text_at(sheet, 0, 0);

    const std::uint32_t below = content.rows + 30;
    const std::uint32_t beside = content.columns + 5;
    set_cell(document, 1, below, R"({"type":"string","text":"below"})");
    set_cell(document, beside, 0, R"({"type":"string","text":"beside"})");
    set_cell(document, beside + 2, below + 3,
             R"({"type":"number","number":7,"text":"7"})");

    const Document saved = reopened(document);
    const Sheet again = first_sheet(saved);
    EXPECT_EQ(text_at(again, 1, below), "below");
    EXPECT_EQ(text_at(again, beside, 0), "beside");
    EXPECT_EQ(again.cell(beside + 2, below + 3).value().number(), 7);
    // what was there stays where it was
    EXPECT_EQ(text_at(again, 0, 0), first);
    const TableDimensions grown = again.content(std::nullopt);
    EXPECT_EQ(grown.rows, below + 4);
    EXPECT_EQ(grown.columns, beside + 3);
  }
}
