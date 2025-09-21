#include <odr/internal/common/file.hpp>
#include <odr/internal/crypto/crypto_util.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_state.hpp>

#include <test_util.hpp>

#include <memory>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::internal::pdf;
using namespace odr::test;

TEST(DocumentParser, foo) {
  const auto file = std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/pdf/style-various-1.pdf"));

  auto in = file->stream();
  DocumentParser parser(*in);

  std::unique_ptr<Document> document = parser.parse_document();

  std::cout << "elements " << document->elements.size() << std::endl;
  std::cout << "pages count " << document->catalog->pages->count << std::endl;

  std::vector<Page *> ordered_pages;
  std::function<void(Pages * pages)> recurse_pages = [&](const Pages *pages) {
    for (Element *kid : pages->kids) {
      if (const auto p = dynamic_cast<Pages *>(kid); p != nullptr) {
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
    std::cout << "page content " << page->contents_reference.front().id
              << std::endl;
    std::cout << "page annotations " << page->annotations.size() << std::endl;
    std::cout << "page resources " << page->resources << std::endl;
  }

  Page *first_page = ordered_pages.front();
  std::string stream;
  for (const auto &content_reference : first_page->contents_reference) {
    IndirectObject page_contents_object = parser.read_object(content_reference);
    stream += parser.read_object_stream(page_contents_object);
  }
  std::string first_page_content = crypto::util::zlib_inflate(stream);

  std::istringstream ss(first_page_content);
  GraphicsOperatorParser parser2(ss);
  GraphicsState state;
  while (!ss.eof()) {
    GraphicsOperator op = parser2.read_operator();
    state.execute(op);

    const std::string &font = state.current().text.font;
    double size = state.current().text.size;

    if (op.type == GraphicsOperatorType::show_text) {
      const std::string &glyphs = op.arguments[0].as_string();
      std::string unicode =
          first_page->resources->font.at(font)->cmap.translate_string(glyphs);
      std::cout << "show text: font=" << font << ", size=" << size
                << ", text=" << unicode << std::endl;
    } else if (op.type == GraphicsOperatorType::show_text_manual_spacing) {
      for (const auto &element : op.arguments[0].as_array()) {
        if (element.is_real()) {
          std::cout << "spacing: " << element.as_real() << std::endl;
        } else if (element.is_string()) {
          const std::string &glyphs = element.as_string();
          std::string unicode =
              first_page->resources->font.at(font)->cmap.translate_string(
                  glyphs);
          std::cout << "show text manual spacing: font=" << font
                    << ", size=" << size << ", text=" << unicode << std::endl;
        }
      }
    } else if (op.type == GraphicsOperatorType::show_text_next_line) {
      std::cout << "TODO show_text_next_line" << std::endl;
    } else if (op.type ==
               GraphicsOperatorType::show_text_next_line_set_spacing) {
      std::cout << "TODO show_text_next_line_set_spacing" << std::endl;
    }
  }
}
