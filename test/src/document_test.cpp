#include <gtest/gtest.h>
#include <odr/document.h>
#include <odr/file.h>
#include <test_util.h>

using namespace odr;
using namespace odr::test;

TEST(DocumentTest, odt) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"));

  EXPECT_EQ(document_file.file_type(), FileType::OPENDOCUMENT_TEXT);

  Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::TEXT);

  auto cursor = document.root_element();

  auto page_layout =
      cursor.element().style(StyleContext::style_tree).page_layout();
  EXPECT_TRUE(page_layout.width().value());
  EXPECT_EQ("8.2673in", page_layout.width().value().value());
  EXPECT_TRUE(page_layout.height().value());
  EXPECT_EQ("11.6925in", page_layout.height().value().value());
  EXPECT_TRUE(page_layout.margin().top().value());
  EXPECT_EQ("0.7874in", page_layout.margin().top().value().value());
}

TEST(DocumentTest, odg) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odg/sample.odg"));

  EXPECT_EQ(document_file.file_type(), FileType::OPENDOCUMENT_GRAPHICS);

  Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::DRAWING);

  auto cursor = document.root_element();

  // TODO
  // EXPECT_EQ(drawing.page_count(), 3);

  cursor.for_each_child([](DocumentCursor &cursor, const std::uint32_t) {
    auto page_layout =
        cursor.element().style(StyleContext::style_tree).page_layout();
    EXPECT_TRUE(page_layout.width().value());
    EXPECT_EQ("21cm", page_layout.width().value().value());
    EXPECT_TRUE(page_layout.height().value());
    EXPECT_EQ("29.7cm", page_layout.height().value().value());
    EXPECT_TRUE(page_layout.margin().top().value());
    EXPECT_EQ("1cm", page_layout.margin().top().value().value());
  });
}
