#include <odr/internal/odf/odf_document.hpp>
#include <odr/internal/odf/odf_element_registry.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/abstract/document.hpp>
#include <odr/internal/abstract/file.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/open_strategy.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

std::string flat_sheet(const std::string &body) {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.spreadsheet">)"
         R"(<office:body><office:spreadsheet>)"
         R"(<table:table table:name="s">)" +
         body +
         R"(</table:table></office:spreadsheet></office:body></office:document>)";
}

/// One row repeated @p rows_repeated times holding one cell repeated
/// @p columns_repeated times, all of it non-empty.
std::string repeated_rows(const std::uint32_t rows_repeated,
                          const std::uint32_t columns_repeated) {
  return R"(<table:table-column table:number-columns-repeated=")" +
         std::to_string(columns_repeated) +
         R"("/>)"
         R"(<table:table-row table:number-rows-repeated=")" +
         std::to_string(rows_repeated) +
         R"("><table:table-cell table:number-columns-repeated=")" +
         std::to_string(columns_repeated) +
         R"("><text:p>x</text:p></table:table-cell></table:table-row>)";
}

std::shared_ptr<abstract::Document> document_of(const std::string &source) {
  const std::unique_ptr<abstract::DocumentFile> file =
      open_strategy::open_document_file(std::make_shared<MemoryFile>(source),
                                        Logger::null());
  return file->document();
}

} // namespace

/// Both repeats are legal, and expanding them would ask for three billion
/// elements from four hundred bytes.
TEST(OdfSheetRepeat, a_repeated_cell_is_one_element) {
  const std::string source = flat_sheet(repeated_rows(1048576, 1024));
  const std::shared_ptr<abstract::Document> held = document_of(source);
  const auto *document = dynamic_cast<const odf::Document *>(held.get());
  ASSERT_NE(document, nullptr);

  EXPECT_LT(document->element_registry().size(), 16);
}

TEST(OdfSheetRepeat, a_repeated_cell_reads_at_every_position_it_covers) {
  const std::string source = flat_sheet(repeated_rows(4, 3));
  const std::shared_ptr<abstract::Document> held = document_of(source);

  const odr::Document public_document(held);
  const Sheet sheet =
      (*public_document.root_element().children().begin()).as_sheet();

  EXPECT_EQ(sheet.dimensions().rows, 4);
  EXPECT_EQ(sheet.dimensions().columns, 3);

  for (std::uint32_t row = 0; row < 4; ++row) {
    for (std::uint32_t column = 0; column < 3; ++column) {
      const SheetCell cell = sheet.cell(column, row);
      ASSERT_TRUE(cell) << column << "," << row;
      const Element paragraph = *cell.children().begin();
      EXPECT_EQ((*paragraph.children().begin()).as_text().content(), "x");
    }
  }
}
