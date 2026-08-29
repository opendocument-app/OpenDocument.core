#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/open_strategy.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

void insert(zip::ZipArchive &zip, const std::string &path,
            const std::string &content) {
  zip.insert_file(std::end(zip), RelPath(path),
                  std::make_shared<MemoryFile>(content));
}

/// The smallest workbook that opens: one sheet, whose `<sheetData>` and
/// `<mergeCells>` are @p sheet_data and @p merge_cells.
std::shared_ptr<abstract::File> workbook(const std::string &sheet_data,
                                         const std::string &merge_cells) {
  zip::ZipArchive zip;
  insert(
      zip, "[Content_Types].xml",
      R"(<Types xmlns="http://schemas.openxmlformats.org/package/2006/content-types">)"
      R"(<Override PartName="/xl/workbook.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml"/>)"
      R"(<Override PartName="/xl/worksheets/sheet1.xml" ContentType="application/vnd.openxmlformats-officedocument.spreadsheetml.worksheet+xml"/>)"
      R"(</Types>)");
  insert(
      zip, "_rels/.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/officeDocument" Target="xl/workbook.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/workbook.xml",
      R"(<workbook xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main" )"
      R"(xmlns:r="http://schemas.openxmlformats.org/officeDocument/2006/relationships">)"
      R"(<sheets><sheet name="s" sheetId="1" r:id="rId1"/></sheets></workbook>)");
  insert(
      zip, "xl/_rels/workbook.xml.rels",
      R"(<Relationships xmlns="http://schemas.openxmlformats.org/package/2006/relationships">)"
      R"(<Relationship Id="rId1" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/worksheet" Target="worksheets/sheet1.xml"/>)"
      R"(</Relationships>)");
  insert(
      zip, "xl/styles.xml",
      R"(<styleSheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main"/>)");
  insert(
      zip, "xl/worksheets/sheet1.xml",
      R"(<worksheet xmlns="http://schemas.openxmlformats.org/spreadsheetml/2006/main">)"
      R"(<sheetData>)" +
          sheet_data + R"(</sheetData>)" + merge_cells + R"(</worksheet>)");

  std::stringstream out;
  zip.save(out);
  return std::make_shared<MemoryFile>(out.str());
}

Sheet first_sheet(const Document &document) {
  return (*document.root_element().children().begin()).as_sheet();
}

Document decode(const std::shared_ptr<abstract::File> &file) {
  return Document(
      open_strategy::open_document_file(file, Logger::null())->document());
}

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

/// A `ref` may name any range the grid allows, and the whole grid is 17 billion
/// positions - so the covered cells have to be found by walking what was read,
/// not by visiting every position the range names.
TEST(OoxmlSpreadsheetMerge,
     a_merge_over_the_whole_grid_is_bounded_by_the_file) {
  const Document document = decode(
      workbook(two_cells,
               R"(<mergeCells><mergeCell ref="A1:XFD1048576"/></mergeCells>)"));
  const Sheet sheet = first_sheet(document);

  EXPECT_FALSE(sheet.cell(0, 0).is_covered());
  EXPECT_TRUE(sheet.cell(1, 0).is_covered());
}
