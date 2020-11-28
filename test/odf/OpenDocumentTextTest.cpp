#include <gtest/gtest.h>
#include <odr/File.h>
#include <odr/Document.h>
#include <odr/DocumentElements.h>

using namespace odr;

TEST(OpenDocumentText, hello) {
  DocumentFile documentFile("/home/andreas/workspace/OpenDocument.test/odt/test.odt");

  std::cout << (int) documentFile.fileType() << std::endl;

  Document document = documentFile.document();

  std::cout << (int) document.documentType() << std::endl;

  auto textDocument = document.textDocument();

  auto props = textDocument.pageProperties();

  std::cout << props.width << " " << props.height << std::endl;
  std::cout << props.marginTop << std::endl;

  for (auto &&e : textDocument.content()) {
    std::cout << (int) e.type() << std::endl;
  }
}
