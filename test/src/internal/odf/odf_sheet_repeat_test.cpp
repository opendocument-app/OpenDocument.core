#include <odr/internal/odf/odf_document.hpp>
#include <odr/internal/odf/odf_element_registry.hpp>

#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/table_dimension.hpp>
#include <odr/table_position.hpp>

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
  const DocumentFile file =
      DecodedFile(open_strategy::open_file(std::make_shared<MemoryFile>(source),
                                           {}, Logger::null()))
          .as_document_file();
  return file.impl()->document();
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

/// The one element cannot say which position a handle means, so the id does.
TEST(OdfSheetRepeat, a_repeated_cell_reports_the_position_it_was_asked_for) {
  const std::shared_ptr<abstract::Document> held =
      document_of(flat_sheet(repeated_rows(4, 3)));
  const Sheet sheet =
      (*odr::Document(held).root_element().children().begin()).as_sheet();

  for (std::uint32_t row = 0; row < 4; ++row) {
    for (std::uint32_t column = 0; column < 3; ++column) {
      const TablePosition position = sheet.cell(column, row).position();
      EXPECT_EQ(position.column, column);
      EXPECT_EQ(position.row, row);
    }
  }
}

TEST(OdfSheetRepeat, two_positions_of_one_run_are_not_the_same_handle) {
  const std::shared_ptr<abstract::Document> held =
      document_of(flat_sheet(repeated_rows(4, 3)));
  const Sheet sheet =
      (*odr::Document(held).root_element().children().begin()).as_sheet();

  EXPECT_NE(sheet.cell(0, 0), sheet.cell(2, 0));
  EXPECT_EQ(sheet.cell(2, 0), sheet.cell(2, 0));
}

/// `DocumentPath` spells a cell by position, so it names the one asked for.
TEST(OdfSheetRepeat, a_repeated_cell_round_trips_through_its_path) {
  const std::shared_ptr<abstract::Document> held =
      document_of(flat_sheet(repeated_rows(4, 3)));
  const odr::Document document(held);
  const Sheet sheet = (*document.root_element().children().begin()).as_sheet();

  const SheetCell cell = sheet.cell(2, 1);

  EXPECT_EQ(document.root_element().navigate_path(cell.document_path()), cell);
}

/// The position stops at the cell: one run stands for every position, so a
/// path into a repeated cell names the anchor.
TEST(OdfSheetRepeat, the_children_of_a_repeated_cell_are_shared) {
  const std::shared_ptr<abstract::Document> held =
      document_of(flat_sheet(repeated_rows(4, 3)));
  const odr::Document document(held);
  const Sheet sheet = (*document.root_element().children().begin()).as_sheet();

  const Element text =
      *(*sheet.cell(2, 1).children().begin()).children().begin();
  EXPECT_EQ(*(*sheet.cell(0, 0).children().begin()).children().begin(), text);
  EXPECT_EQ(document.root_element().navigate_path(text.document_path()), text);
}
