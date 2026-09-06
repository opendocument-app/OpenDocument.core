#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/table_dimension.hpp>

#include <internal/ooxml/ooxml_spreadsheet_test_util.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace odr;
using namespace odr::test::ooxml;

namespace {

constexpr const char *two_cells =
    R"(<row r="1"><c r="A1" t="inlineStr"><is><t>a</t></is></c>)"
    R"(<c r="B1" t="inlineStr"><is><t>b</t></is></c></row>)";

} // namespace

TEST(OoxmlSpreadsheetMerge, a_merge_covers_the_cells_it_spans) {
  const Document document = decode(workbook(
      two_cells, R"(<mergeCells><mergeCell ref="A1:B1"/></mergeCells>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_EQ(sheet.cell(0, 0).span().columns, 2);
  EXPECT_FALSE(sheet.cell(0, 0).is_covered());
  EXPECT_TRUE(sheet.cell(1, 0).is_covered());
}

/// The whole grid is 17 billion positions, so the covered cells have to be
/// found by walking what was read.
TEST(OoxmlSpreadsheetMerge,
     a_merge_over_the_whole_grid_is_bounded_by_the_file) {
  const Document document = decode(
      workbook(two_cells,
               R"(<mergeCells><mergeCell ref="A1:XFD1048576"/></mergeCells>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_FALSE(sheet.cell(0, 0).is_covered());
  EXPECT_TRUE(sheet.cell(1, 0).is_covered());
}
