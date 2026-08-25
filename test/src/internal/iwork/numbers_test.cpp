#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/iwork/iwork_document.hpp>
#include <odr/internal/iwork/iwork_file.hpp>
#include <odr/internal/iwork/iwork_table.hpp>

#include <test_util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>
#include <vector>

using namespace odr;
using odr::test::TestData;
namespace iwork = odr::internal::iwork;

namespace {

/// The text of a cell, its paragraphs joined by newlines.
std::string cell_text(const Element cell) {
  std::string result;
  for (const Element paragraph : cell.children()) {
    if (!result.empty()) {
      result += '\n';
    }
    for (const Element child : paragraph.children()) {
      if (child.type() == ElementType::line_break) {
        result += '\n';
      } else {
        result += child.as_text().content();
      }
    }
  }
  return result;
}

/// A sheet's content extent, row by row.
std::vector<std::vector<std::string>> grid(const Sheet sheet) {
  const TableDimensions content = sheet.content({});

  std::vector<std::vector<std::string>> result;
  for (std::uint32_t row = 0; row < content.rows; ++row) {
    std::vector<std::string> cells;
    for (std::uint32_t column = 0; column < content.columns; ++column) {
      cells.push_back(cell_text(sheet.cell(column, row)));
    }
    result.push_back(std::move(cells));
  }
  return result;
}

std::vector<std::string> sheet_names(const Element root) {
  std::vector<std::string> result;
  for (const Element sheet : root.children()) {
    EXPECT_EQ(sheet.type(), ElementType::sheet);
    result.push_back(sheet.as_sheet().name());
  }
  return result;
}

Sheet sheet_at(const Element root, const std::size_t index) {
  std::size_t i = 0;
  for (const Element sheet : root.children()) {
    if (i++ == index) {
      return sheet.as_sheet();
    }
  }
  return {};
}

} // namespace

TEST(IworkNumbers, is_detected_by_content) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);
  const std::string path =
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers");

  EXPECT_THAT(list_file_types(path, logger),
              testing::Contains(FileType::iwork_numbers));

  const DecodedFile file(path, logger);
  EXPECT_EQ(file.file_type(), FileType::iwork_numbers);
  EXPECT_EQ(file.file_category(), FileCategory::document);
  EXPECT_EQ(file.as_document_file().document_type(), DocumentType::spreadsheet);
}

// The blank template's one sheet holds one table with nothing in it, which
// must come back as a sheet of its declared extent and no content.
TEST(IworkNumbers, empty) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/empty.numbers"),
      Logger::null());
  EXPECT_EQ(document_file.file_type(), FileType::iwork_numbers);

  const Document document = document_file.document();
  EXPECT_EQ(document.document_type(), DocumentType::spreadsheet);
  EXPECT_FALSE(document.is_editable());

  const Element root = document.root_element();
  EXPECT_EQ(sheet_names(root), (std::vector<std::string>{"Sheet 1 – Table 1"}));

  const Sheet sheet = sheet_at(root, 0);
  EXPECT_EQ(sheet.dimensions().rows, 22);
  EXPECT_EQ(sheet.dimensions().columns, 7);
  EXPECT_EQ(sheet.content({}).rows, 0);
  EXPECT_EQ(sheet.content({}).columns, 0);
}

// A Numbers sheet holds many tables and our `Sheet` is one grid, so each
// table is a sheet of its own rather than only the first one surviving.
TEST(IworkNumbers, one_sheet_per_table) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers"),
      Logger::null());
  const Document document = document_file.document();

  EXPECT_EQ(sheet_names(document.root_element()),
            (std::vector<std::string>{"Sales – Quarterly", "Sales – Wide",
                                      "Types – Values"}));
}

TEST(IworkNumbers, cell_values) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers"),
      Logger::null());
  const Document document = document_file.document();

  EXPECT_EQ(grid(sheet_at(document.root_element(), 0)),
            (std::vector<std::vector<std::string>>{
                {"Quarter", "Revenue", "Growth"},
                {"Q1", "1000", "0.125"},
                {"Q2", "1250.5", "0.25"},
                // the cached value of `=SUM(B2:B3)` and `=AVERAGE(C2:C3)`;
                // `CalculationEngine.iwa` is not read
                {"Total", "2250.5", "0.1875"},
            }));
}

// Rows and columns cannot be confused: this table is three rows of six
// columns, and holds cells only at its corners.
TEST(IworkNumbers, a_table_wider_than_it_is_tall) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers"),
      Logger::null());
  const Document document = document_file.document();

  const Sheet sheet = sheet_at(document.root_element(), 1);
  EXPECT_EQ(sheet.dimensions().rows, 3);
  EXPECT_EQ(sheet.dimensions().columns, 6);

  EXPECT_EQ(grid(sheet), (std::vector<std::vector<std::string>>{
                             {"a", "", "", "", "", "f"},
                             {"", "", "", "", "", ""},
                             {"bottom left", "", "", "", "", "bottom right"},
                         }));
}

TEST(IworkNumbers, every_cell_type_the_fixtures_hold) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers"),
      Logger::null());
  const Document document = document_file.document();

  EXPECT_EQ(grid(sheet_at(document.root_element(), 2)),
            (std::vector<std::vector<std::string>>{
                {"Kind", "Value"},
                {"text", "A cell holding a rather longer piece of text"},
                {"number", "-42"},
                {"boolean", "TRUE"},
                {"date", "2024-01-01T10:30:00Z"},
                {"duration", "1h 30m"},
                // a row whose second cell the tile does not carry at all
                {"empty", ""},
                // Numbers shows 7.5%; the number format is not applied
                {"percent", "0.075"},
                {"two lines", "line one\nline two"},
            }));
}

// A number cell is right-aligned by the renderer; nothing else is.
TEST(IworkNumbers, only_a_number_reports_a_float_value_type) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/numbers/style-various-1.numbers"),
      Logger::null());
  const Document document = document_file.document();
  const Sheet sheet = sheet_at(document.root_element(), 2);

  EXPECT_EQ(sheet.cell(1, 2).value_type(), ValueType::float_number);
  EXPECT_EQ(sheet.cell(1, 1).value_type(), ValueType::string);
  EXPECT_EQ(sheet.cell(1, 4).value_type(), ValueType::string);
  // a position the tile carries no cell for
  EXPECT_EQ(sheet.cell(1, 6).value_type(), ValueType::unknown);
}

// A spreadsheet value is a decimal by construction, which is why Apple stores
// one; going through a `double` would put back the rounding it avoids.
TEST(IworkDecimal128, reads_the_values_the_fixtures_hold) {
  const auto decimal = [](const std::string &hex) {
    std::string bytes;
    for (std::size_t i = 0; i < hex.size(); i += 2) {
      bytes.push_back(
          static_cast<char>(std::stoul(hex.substr(i, 2), nullptr, 16)));
    }
    return iwork::decimal128_to_string(bytes);
  };

  EXPECT_EQ(decimal("e8030000000000000000000000004030"), "1000");
  EXPECT_EQ(decimal("7d000000000000000000000000003a30"), "0.125");
  EXPECT_EQ(decimal("d9300000000000000000000000003e30"), "1250.5");
  EXPECT_EQ(decimal("2a000000000000000000000000004030"), "42");
  EXPECT_EQ(decimal("2a0000000000000000000000000040b0"), "-42");
  // Numbers stores 0.075 as 750000000000000e-16
  EXPECT_EQ(decimal("00e094fb1eaa02000000000000002030"), "0.075");
  EXPECT_EQ(decimal("00000000000000000000000000004030"), "0");
}

TEST(IworkDecimal128, is_cut_off) {
  EXPECT_ANY_THROW(std::ignore = iwork::decimal128_to_string("short"));
}

// The instant stored is UTC, whatever the clock said when it was typed.
TEST(IworkDate, reads_seconds_since_2001) {
  EXPECT_EQ(iwork::date_to_string(0), "2001-01-01T00:00:00Z");
  EXPECT_EQ(iwork::date_to_string(725797800), "2024-01-01T10:30:00Z");
  EXPECT_EQ(iwork::date_to_string(-1), "2000-12-31T23:59:59Z");
}

TEST(IworkDuration, reads_a_count_of_seconds) {
  EXPECT_EQ(iwork::duration_to_string(5400), "1h 30m");
  EXPECT_EQ(iwork::duration_to_string(0), "0s");
  EXPECT_EQ(iwork::duration_to_string(90061), "1d 1h 1m 1s");
  EXPECT_EQ(iwork::duration_to_string(-60), "-1m");
}
