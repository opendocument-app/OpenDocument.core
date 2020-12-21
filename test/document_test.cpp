#include <gtest/gtest.h>
#include <odr/document.h>
#include <odr/document_elements.h>
#include <odr/document_style.h>
#include <odr/file.h>

using namespace odr;

TEST(DocumentTest, odf_hello) {
  DocumentFile documentFile(
      "/home/andreas/workspace/OpenDocument.test/odt/about.odt");

  std::cout << (int)documentFile.fileType() << std::endl;

  Document document = documentFile.document();

  std::cout << (int)document.documentType() << std::endl;

  auto textDocument = document.textDocument();

  auto pageStyle = textDocument.pageStyle();

  std::cout << *pageStyle.width() << " " << *pageStyle.height() << std::endl;
  std::cout << *pageStyle.marginTop() << std::endl;

  for (auto &&e : textDocument.content()) {
    std::cout << (int)e.type() << std::endl;
  }
}
