#include <gtest/gtest.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>

using namespace odr;

TEST(OpenDocumentText, hello) {
  Document document("/home/andreas/workspace/OpenDocument.test/odt/test.odt");

  std::cout << (int) document.fileType() << std::endl;
  std::cout << (int) document.documentType() << std::endl;

  TextDocument textDocument(std::move(document));

  auto props = textDocument.pageProperties();

  std::cout << props.width << " " << props.height << std::endl;
  std::cout << props.marginTop << std::endl;

  for (auto e = textDocument.firstContentElement(); e; e = e->nextSibling()) {
    std::cout << (int) e->type() << std::endl;
  }
}
