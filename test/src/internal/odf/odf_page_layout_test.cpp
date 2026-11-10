#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace odr;

namespace {

/// A flat text document whose one master page uses a layout of
/// @p properties.
std::string flat_odt(const std::string &properties) {
  return R"(<?xml version="1.0" encoding="UTF-8"?>)"
         R"(<office:document office:mimetype=")"
         R"(application/vnd.oasis.opendocument.text">)"
         R"(<office:automatic-styles><style:page-layout style:name="pm1">)"
         R"(<style:page-layout-properties )" +
         properties +
         R"(/></style:page-layout></office:automatic-styles>)"
         R"(<office:master-styles><style:master-page style:name="Standard" )"
         R"(style:page-layout-name="pm1"/></office:master-styles>)"
         R"(<office:body><office:text><text:p>Text</text:p></office:text>)"
         R"(</office:body></office:document>)";
}

PageLayout page_layout_of(const std::string &source) {
  const Document document =
      odr::open(File::from_memory(source)).as_document_file().document();
  return document.root_element().as_text_root().page_layout();
}

} // namespace

TEST(OdfPageLayout, a_layout_stating_no_size_takes_a4) {
  const PageLayout layout =
      page_layout_of(flat_odt(R"(fo:margin-left="4.5cm")"));

  ASSERT_TRUE(layout.width.has_value());
  ASSERT_TRUE(layout.height.has_value());
  EXPECT_EQ(layout.width->to_string(), "21cm");
  EXPECT_EQ(layout.height->to_string(), "29.7cm");
  EXPECT_EQ(layout.margin.left->to_string(), "4.5cm");
}

TEST(OdfPageLayout, a_landscape_layout_stating_no_size_stays_upright) {
  const PageLayout layout =
      page_layout_of(flat_odt(R"(style:print-orientation="landscape")"));

  ASSERT_TRUE(layout.width.has_value());
  ASSERT_TRUE(layout.height.has_value());
  EXPECT_EQ(layout.width->to_string(), "21cm");
  EXPECT_EQ(layout.height->to_string(), "29.7cm");
}

TEST(OdfPageLayout, a_missing_width_or_height_is_completed_on_its_own) {
  const PageLayout layout =
      page_layout_of(flat_odt(R"(fo:page-width="8.5in")"));

  ASSERT_TRUE(layout.width.has_value());
  ASSERT_TRUE(layout.height.has_value());
  EXPECT_EQ(layout.width->to_string(), "8.5in");
  EXPECT_EQ(layout.height->to_string(), "29.7cm");
}

TEST(OdfPageLayout, a_stated_size_is_kept) {
  const PageLayout layout = page_layout_of(
      flat_odt(R"(fo:page-width="8.5in" fo:page-height="11in")"));

  ASSERT_TRUE(layout.width.has_value());
  ASSERT_TRUE(layout.height.has_value());
  EXPECT_EQ(layout.width->to_string(), "8.5in");
  EXPECT_EQ(layout.height->to_string(), "11in");
}

TEST(OdfPageLayout, a_drawing_page_whose_master_names_no_layout_takes_a4) {
  // As LibreOffice's BPMN template writes it: a master page with no
  // `style:page-layout-name` at all.
  const std::string source =
      R"(<?xml version="1.0" encoding="UTF-8"?>)"
      R"(<office:document office:mimetype=")"
      R"(application/vnd.oasis.opendocument.graphics">)"
      R"(<office:master-styles><style:master-page style:name="Standard"/>)"
      R"(</office:master-styles><office:body><office:drawing>)"
      R"(<draw:page draw:name="page1" draw:master-page-name="Standard"/>)"
      R"(</office:drawing></office:body></office:document>)";

  const Document document =
      odr::open(File::from_memory(source)).as_document_file().document();
  const PageLayout layout =
      (*document.root_element().children().begin()).as_page().page_layout();

  ASSERT_TRUE(layout.width.has_value());
  ASSERT_TRUE(layout.height.has_value());
  EXPECT_EQ(layout.width->to_string(), "21cm");
  EXPECT_EQ(layout.height->to_string(), "29.7cm");
}
