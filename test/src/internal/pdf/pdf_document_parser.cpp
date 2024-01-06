#include <odr/internal/common/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_state.hpp>

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
  DocumentParser parser(*in);

  std::unique_ptr<Document> document = parser.parse_document();

  std::cout << "elements " << document->elements.size() << std::endl;
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
    std::cout << "page annotations " << page->annotations.size() << std::endl;
    std::cout << "page resources " << page->resources << std::endl;
  }

  Page *first_page = ordered_pages.front();
  for (const auto &[key, value] : first_page->resources->font) {
    std::cout << "font " << key << std::endl;
    for (const auto &[prop_key, prop_val] : value->object.as_dictionary()) {
      std::cout << "prop key " << prop_key << std::endl;
    }
    auto to_unicode_ref =
        value->object.as_dictionary()["ToUnicode"].as_reference();
    std::cout << "to unicode " << to_unicode_ref.first << std::endl;
    auto to_unicode_obj = parser.read_object(to_unicode_ref);
    std::cout << "to unicode " << to_unicode_obj.object.as_dictionary().size()
              << std::endl;
    for (const auto &[prop_key, prop_val] :
         to_unicode_obj.object.as_dictionary()) {
      std::cout << "prop key " << prop_key << std::endl;
    }
    std::string stream = parser.read_object_stream(to_unicode_obj);
    std::cout << crypto::util::zlib_inflate(stream) << std::endl;
  }

  IndirectObject first_page_contents_object =
      parser.read_object(first_page->contents_reference);
  std::string stream = parser.read_object_stream(first_page_contents_object);
  std::string first_page_content = crypto::util::zlib_inflate(stream);

  std::cout << first_page_content << std::endl;

  std::istringstream in2(first_page_content);
  GraphicsOperatorParser parser2(in2);
  GraphicsState state;
  while (!in2.eof()) {
    GraphicsOperator op = parser2.read_operator();
    state.execute(op);
  }
}
