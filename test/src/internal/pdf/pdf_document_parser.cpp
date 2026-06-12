#include <odr/internal/common/file.hpp>
#include <odr/internal/pdf/pdf_document.hpp>
#include <odr/internal/pdf/pdf_document_element.hpp>
#include <odr/internal/pdf/pdf_document_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_operator.hpp>
#include <odr/internal/pdf/pdf_graphics_operator_parser.hpp>
#include <odr/internal/pdf/pdf_graphics_state.hpp>

#include <test_util.hpp>

#include "pdf_test_file_builder.hpp"

#include <memory>
#include <sstream>

#include <gtest/gtest.h>

using namespace odr::internal;
using namespace odr::internal::pdf;
using namespace odr::test;
using PdfFileBuilder = odr::test::pdf::PdfFileBuilder;

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
  std::string first_page_content;
  for (const auto &content_reference : first_page->contents_reference) {
    first_page_content += parser.read_decoded_stream(content_reference);
    first_page_content += '\n';
  }

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

namespace {

std::string two_object_mini_pdf(PdfFileBuilder &builder, const bool classic) {
  builder.object("<< /Type /Catalog /Pages 2 0 R >>")
      .object("<< /Type /Pages /Kids [3 0 R] /Count 1 >>")
      .object("<< /Type /Page /Parent 2 0 R /MediaBox [0 0 612 792] "
              "/Resources << >> /Contents 4 0 R >>")
      .stream_object("", "BT ET")
      .trailer("/Root 1 0 R");
  return classic ? builder.build_classic() : builder.build_xref_stream();
}

void check_mini_pdf(const std::string &pdf) {
  std::istringstream in(pdf);
  DocumentParser parser(in);

  std::unique_ptr<Document> document = parser.parse_document();

  ASSERT_EQ(document->catalog->pages->count, 1);
  ASSERT_EQ(document->catalog->pages->kids.size(), 1);
  auto *page = dynamic_cast<Page *>(document->catalog->pages->kids.front());
  ASSERT_NE(page, nullptr);
  ASSERT_EQ(page->contents_reference.size(), 1);
  EXPECT_EQ(parser.read_decoded_stream(page->contents_reference.front()),
            "BT ET");
}

} // namespace

TEST(DocumentParser, mini_pdf_with_classic_xref_table) {
  PdfFileBuilder builder;
  check_mini_pdf(two_object_mini_pdf(builder, true));
}

TEST(DocumentParser, mini_pdf_with_xref_stream) {
  PdfFileBuilder builder;
  const std::string pdf = two_object_mini_pdf(builder, false);

  std::istringstream in(pdf);
  DocumentParser parser(in);
  parser.parse_document();

  // 4 objects, the cross-reference stream itself, and the free head
  EXPECT_EQ(parser.xref().table.size(), 6);
  EXPECT_TRUE(parser.xref().table.at(ObjectReference(5, 0)).is_used());

  check_mini_pdf(pdf);
}

namespace {

void check_fixture_parses(const std::string &short_path) {
  SCOPED_TRACE(short_path);

  const auto file =
      std::make_shared<DiskFile>(TestData::test_file_path(short_path));

  auto in = file->stream();
  DocumentParser parser(*in);

  std::unique_ptr<Document> document = parser.parse_document();

  std::vector<Page *> ordered_pages;
  std::function<void(Pages * pages)> recurse_pages = [&](const Pages *pages) {
    for (Element *kid : pages->kids) {
      if (const auto p = dynamic_cast<Pages *>(kid); p != nullptr) {
        recurse_pages(p);
      } else if (auto page = dynamic_cast<Page *>(kid); page != nullptr) {
        ordered_pages.push_back(page);
      } else {
        FAIL() << "unhandled element";
      }
    }
  };
  recurse_pages(document->catalog->pages);

  EXPECT_FALSE(ordered_pages.empty());

  for (const Page *page : ordered_pages) {
    for (const auto &content_reference : page->contents_reference) {
      EXPECT_FALSE(parser.read_decoded_stream(content_reference).empty());
    }
  }
}

} // namespace

TEST(DocumentParser, classic_xref_fixture) {
  check_fixture_parses("odr-public/pdf/style-various-1.pdf");
}
