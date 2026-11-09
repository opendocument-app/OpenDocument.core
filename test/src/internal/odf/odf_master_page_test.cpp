#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/common/path.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>

using namespace odr;
using namespace odr::internal;

namespace {

/// A text document with a first page and a standard page, each with a
/// header, the standard one with a footer as well.
std::string write_odt(const std::string &name) {
  const std::string namespaces =
      R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" )"
      R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" )"
      R"(xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0" )"
      R"(xmlns:svg="urn:oasis:names:tc:opendocument:xmlns:svg-compatible:1.0")";
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-content )" +
      namespaces +
      R"(><office:automatic-styles><style:style style:name="P1" )"
      R"(style:family="paragraph" style:master-page-name="First"/>)"
      R"(</office:automatic-styles><office:body><office:text>)"
      R"(<text:p text:style-name="P1">Body</text:p></office:text>)"
      R"(</office:body></office:document-content>)";
  const std::string styles =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-styles )" +
      namespaces +
      R"(><office:automatic-styles><style:page-layout style:name="pm1">)"
      R"(<style:page-layout-properties fo:page-width="21cm" )"
      R"(fo:page-height="29.7cm"/><style:header-style>)"
      R"(<style:header-footer-properties fo:min-height="0.6cm" )"
      R"(fo:margin-bottom="0.5cm"/></style:header-style><style:footer-style>)"
      R"(<style:header-footer-properties svg:height="1cm" )"
      R"(fo:margin-top="0.3cm"/></style:footer-style></style:page-layout>)"
      R"(</office:automatic-styles><office:master-styles>)"
      R"(<style:master-page style:name="Standard" style:page-layout-name="pm1">)"
      R"(<style:header><text:p>Standard header</text:p></style:header>)"
      R"(<style:footer><text:p>Standard footer</text:p></style:footer>)"
      R"(</style:master-page>)"
      R"(<style:master-page style:name="First" style:page-layout-name="pm1" )"
      R"(style:next-style-name="Standard">)"
      R"(<style:header><text:p>First header</text:p></style:header>)"
      R"(</style:master-page></office:master-styles></office:document-styles>)";

  const std::string path = (std::filesystem::current_path() / name).string();

  zip::ZipArchive zip;
  zip.insert_file(
      std::end(zip), RelPath("mimetype"),
      std::make_shared<MemoryFile>("application/vnd.oasis.opendocument.text"));
  zip.insert_file(std::end(zip), RelPath("content.xml"),
                  std::make_shared<MemoryFile>(content));
  zip.insert_file(std::end(zip), RelPath("styles.xml"),
                  std::make_shared<MemoryFile>(styles));
  std::ofstream out(path, std::ios::binary);
  zip.save(out);

  return path;
}

std::string text_of(const Element &element) {
  std::string result;
  for (const Element child : element.children()) {
    if (child.type() == ElementType::text) {
      result += child.as_text().content();
    }
    result += text_of(child);
  }
  return result;
}

} // namespace

TEST(OdfMasterPage, the_header_and_footer_are_their_own_elements) {
  const Document document = odr::open(write_odt("odf_master_page_header.odt"))
                                .as_document_file()
                                .document();
  const MasterPage first =
      document.root_element().as_text_root().first_master_page();

  ASSERT_TRUE(first.header());
  EXPECT_EQ(text_of(first.header()), "First header");
  EXPECT_FALSE(first.footer());

  const MasterPage standard = first.next_master_page();
  EXPECT_EQ(text_of(standard.header()), "Standard header");
  EXPECT_EQ(text_of(standard.footer()), "Standard footer");
  // naming no next page keeps this one
  EXPECT_EQ(text_of(standard.next_master_page().header()), "Standard header");
}

TEST(OdfMasterPage, the_layout_carries_the_header_and_footer_boxes) {
  const Document document = odr::open(write_odt("odf_master_page_layout.odt"))
                                .as_document_file()
                                .document();
  const PageLayout layout =
      document.root_element().as_text_root().page_layout();

  EXPECT_EQ(layout.header_min_height->to_string(), "0.6cm");
  EXPECT_EQ(layout.header_spacing->to_string(), "0.5cm");
  EXPECT_EQ(layout.footer_min_height->to_string(), "1cm");
  EXPECT_EQ(layout.footer_spacing->to_string(), "0.3cm");
}
