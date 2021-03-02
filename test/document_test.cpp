#include <gtest/gtest.h>
#include <odr/document.h>
#include <odr/document_elements.h>
#include <odr/document_style.h>
#include <odr/file.h>
#include <test/test_util.h>

using namespace odr;
using namespace odr::test;

TEST(DocumentTest, odt) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"));

  EXPECT_EQ(document_file.file_type(), FileType::OPENDOCUMENT_TEXT);

  Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::TEXT);

  auto text_document = document.text_tocument();

  auto page_style = text_document.page_style();

  std::cout << *page_style.width() << " " << *page_style.height() << std::endl;
  std::cout << *page_style.margin_top() << std::endl;

  for (auto &&e : text_document.content()) {
    std::cout << (int)e.type() << std::endl;
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

    std::cout << *page_style.width() << " " << *page_style.height()
              << std::endl;
    std::cout << *page_style.margin_top() << std::endl;
  }
}
