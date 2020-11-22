#include <common/AbstractDocument.h>
#include <gtest/gtest.h>
#include <odf/OpenDocument.h>
#include <odr/Document.h>

using namespace odr;
using namespace odr::odf;

TEST(OpenDocumentText, hello) {
  OpenDocument odf("/home/andreas/workspace/OpenDocument.test/odt/test.odt");
  auto doc = odf.document();

  std::cout << (int) doc->type() << std::endl;
  std::cout << doc->isText() << std::endl;

  auto textdoc = std::dynamic_pointer_cast<common::AbstractTextDocument>(doc);
  auto props = textdoc->pageProperties();

  std::cout << props.width << " " << props.height << std::endl;
  std::cout << props.marginTop << std::endl;

  for (auto iter = textdoc->firstContentElement(); iter; iter = iter->nextSibling()) {
    std::cout << (int) iter->type() << std::endl;
  }
}
