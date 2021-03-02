#include <gtest/gtest.h>
#include <odr/document.h>
#include <odr/document_elements.h>
#include <odr/document_style.h>
#include <odr/file.h>
#include <test/test_util.h>

using namespace odr;
using namespace odr::test;

TEST(DocumentTest, odf_hello) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"));

  std::cout << (int)document_file.file_type() << std::endl;

  Document document = document_file.document();

  std::cout << (int)document.document_type() << std::endl;

  auto text_document = document.text_tocument();

  auto page_style = text_document.page_style();

  std::cout << *page_style.width() << " " << *page_style.height() << std::endl;
  std::cout << *page_style.margin_top() << std::endl;

  for (auto &&e : text_document.content()) {
    std::cout << (int)e.type() << std::endl;
  }
}
