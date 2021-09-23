#include <gtest/gtest.h>
#include <odr/document.h>
#include <odr/file.h>
#include <odr/style.h>
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

  auto page_layout = cursor.element().text_root().page_layout();
  EXPECT_TRUE(page_layout.width);
  EXPECT_EQ(Measure("8.2673in"), page_layout.width);
  EXPECT_TRUE(page_layout.height);
  EXPECT_EQ(Measure("11.6925in"), page_layout.height);
  EXPECT_TRUE(page_layout.margin.top);
  EXPECT_EQ(Measure("0.7874in"), page_layout.margin.top);
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
    auto page_layout = cursor.element().page().page_layout();
    EXPECT_TRUE(page_layout.width);
    EXPECT_EQ(Measure("21cm"), page_layout.width);
    EXPECT_TRUE(page_layout.height);
    EXPECT_EQ(Measure("29.7cm"), page_layout.height);
    EXPECT_TRUE(page_layout.margin.top);
    EXPECT_EQ(Measure("1cm"), page_layout.margin.top);
  });
}

TEST(DocumentTest, edit_odt) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"));

  EXPECT_EQ(document_file.file_type(), FileType::OPENDOCUMENT_TEXT);

  Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::TEXT);

  auto cursor = document.root_element();

  DocumentCursor::ChildVisitor edit = [&](DocumentCursor &cursor,
                                          const std::uint32_t) {
    cursor.for_each_child(edit);

    if (auto text = cursor.element().text()) {
      text.text("hello world!");
    }
  };
  edit(cursor, 0);

  document.save("about_edit.odt");
  DocumentFile("about_edit.odt");
}
