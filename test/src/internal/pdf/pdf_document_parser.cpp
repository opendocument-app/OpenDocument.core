#include <odr/internal/common/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_file_parser.hpp>

#include <test_util.hpp>

#include <functional>
#include <memory>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::internal::pdf;
using namespace odr::test;

TEST(DocumentParser, foo) {
  auto file = std::make_shared<common::DiskFile>(
      TestData::test_file_path("odr-public/pdf/style-various-1.pdf"));

  auto in = file->stream();
  FileParser file_parser(*in);
  DocumentParser parser(file_parser);

  std::unique_ptr<Document> document = parser.parse_document();

  std::cout << "elements " << document->element.size() << std::endl;
  std::cout << "pages count " << document->catalog->pages->count << std::endl;

  std::vector<Page *> ordered_pages;
  std::function<void(Pages * pages)> recurse_pages = [&](Pages *pages) {
    for (Element *kid : pages->kids) {
      if (auto p = dynamic_cast<Pages *>(kid); p != nullptr) {
        recurse_pages(p);
      } else if (auto page = dynamic_cast<Page *>(kid); page != nullptr) {
        ordered_pages.push_back(page);
      } else {
        throw std::runtime_error("unhandled element");
      }
    }
  };

  recurse_pages(document->catalog->pages);

  for (Page *page : ordered_pages) {
    std::cout << "page content " << page->contents_reference.first << std::endl;
  }

  IndirectObject first_page_contents_object =
      parser.read_object(ordered_pages.front()->contents_reference);
  std::cout << "first page contents size "
            << first_page_contents_object.stream.value().size() << std::endl;
  std::cout << crypto::util::zlib_inflate(
                   first_page_contents_object.stream.value())
            << std::endl;
}
