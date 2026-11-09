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

/// A text document whose one master page uses a layout of @p properties.
std::string write_odt(const std::string &name, const std::string &properties) {
  const std::string namespaces =
      R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0" )"
      R"(xmlns:text="urn:oasis:names:tc:opendocument:xmlns:text:1.0" )"
      R"(xmlns:fo="urn:oasis:names:tc:opendocument:xmlns:xsl-fo-compatible:1.0")";
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-content )" +
      namespaces +
      R"(><office:body><office:text><text:p>Text</text:p></office:text>)"
      R"(</office:body></office:document-content>)";
  const std::string styles =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-styles )" +
      namespaces +
      R"(><office:automatic-styles><style:page-layout style:name="pm1">)"
      R"(<style:page-layout-properties )" +
      properties +
      R"(/></style:page-layout></office:automatic-styles>)"
      R"(<office:master-styles><style:master-page style:name="Standard" )"
      R"(style:page-layout-name="pm1"/></office:master-styles>)"
      R"(</office:document-styles>)";

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

PageLayout page_layout_of(const std::string &path) {
  const Document document = odr::open(path).as_document_file().document();
  return document.root_element().as_text_root().page_layout();
}

} // namespace

TEST(OdfPageLayout, a_layout_stating_no_size_takes_a4) {
  const PageLayout layout = page_layout_of(
      write_odt("odf_page_layout_no_size.odt", R"(fo:margin-left="4.5cm")"));

  ASSERT_TRUE(layout.width.has_value());
  ASSERT_TRUE(layout.height.has_value());
  EXPECT_EQ(layout.width->to_string(), "21cm");
  EXPECT_EQ(layout.height->to_string(), "29.7cm");
  EXPECT_EQ(layout.margin.left->to_string(), "4.5cm");
}

TEST(OdfPageLayout, a_landscape_layout_stating_no_size_stays_upright) {
  // LibreOffice keeps A4 portrait for it
  const PageLayout layout =
      page_layout_of(write_odt("odf_page_layout_no_size_landscape.odt",
                               R"(style:print-orientation="landscape")"));

  EXPECT_EQ(layout.width->to_string(), "21cm");
  EXPECT_EQ(layout.height->to_string(), "29.7cm");
}

TEST(OdfPageLayout, a_stated_size_is_kept) {
  const PageLayout layout = page_layout_of(
      write_odt("odf_page_layout_letter.odt",
                R"(fo:page-width="8.5in" fo:page-height="11in")"));

  EXPECT_EQ(layout.width->to_string(), "8.5in");
  EXPECT_EQ(layout.height->to_string(), "11in");
}

TEST(OdfPageLayout, a_drawing_page_whose_master_names_no_layout_takes_a4) {
  // As LibreOffice's BPMN template writes it: a master page with no
  // `style:page-layout-name` at all.
  const std::string path =
      (std::filesystem::current_path() / "odf_page_layout_drawing.odg")
          .string();
  const std::string content =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-content )"
      R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:draw="urn:oasis:names:tc:opendocument:xmlns:drawing:1.0">)"
      R"(<office:body><office:drawing><draw:page draw:name="page1" )"
      R"(draw:master-page-name="Standard"/></office:drawing></office:body>)"
      R"(</office:document-content>)";
  const std::string styles =
      R"(<?xml version="1.0" encoding="UTF-8"?><office:document-styles )"
      R"(xmlns:office="urn:oasis:names:tc:opendocument:xmlns:office:1.0" )"
      R"(xmlns:style="urn:oasis:names:tc:opendocument:xmlns:style:1.0">)"
      R"(<office:master-styles><style:master-page style:name="Standard"/>)"
      R"(</office:master-styles></office:document-styles>)";
  zip::ZipArchive zip;
  zip.insert_file(std::end(zip), RelPath("mimetype"),
                  std::make_shared<MemoryFile>(
                      "application/vnd.oasis.opendocument.graphics"));
  zip.insert_file(std::end(zip), RelPath("content.xml"),
                  std::make_shared<MemoryFile>(content));
  zip.insert_file(std::end(zip), RelPath("styles.xml"),
                  std::make_shared<MemoryFile>(styles));
  {
    std::ofstream out(path, std::ios::binary);
    zip.save(out);
  }

  const Document document = odr::open(path).as_document_file().document();
  const PageLayout layout =
      (*document.root_element().children().begin()).as_page().page_layout();

  EXPECT_EQ(layout.width->to_string(), "21cm");
  EXPECT_EQ(layout.height->to_string(), "29.7cm");
}
