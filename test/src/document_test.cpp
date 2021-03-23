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

  auto text_document = document.text_document();

  auto page_style = text_document.page_style();

  EXPECT_TRUE(page_style.width());
  EXPECT_EQ("8.2673in", page_style.width().get_string());
  EXPECT_TRUE(page_style.height());
  EXPECT_EQ("11.6925in", page_style.height().get_string());
  EXPECT_TRUE(page_style.margin_top());
  EXPECT_EQ("0.7874in", page_style.margin_top().get_string());

  for (auto &&e : text_document.content()) {
    EXPECT_TRUE(e);
  }
}

TEST(DocumentTest, odg) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odg/sample.odg"));

  EXPECT_EQ(document_file.file_type(), FileType::OPENDOCUMENT_GRAPHICS);

  Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::DRAWING);

  auto drawing = document.drawing();

  // TODO
  // EXPECT_EQ(drawing.page_count(), 3);

  for (auto &&e : drawing.pages()) {
    auto page_style = e.page_style();

    EXPECT_TRUE(page_style.width());
    EXPECT_EQ("21cm", page_style.width().get_string());
    EXPECT_TRUE(page_style.height());
    EXPECT_EQ("29.7cm", page_style.height().get_string());
    EXPECT_TRUE(page_style.margin_top());
    EXPECT_EQ("1cm", page_style.margin_top().get_string());
  }
}
