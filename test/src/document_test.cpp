#include <gtest/gtest.h>
#include <odr/document.h>
#include <odr/file.h>
#include <test_util.h>

using namespace odr;
using namespace odr::test;

TEST(DocumentTest, odt) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"));

  EXPECT_EQ(document_file.file_type(), FileType::OPENDOCUMENT_TEXT);

  Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::TEXT);

  auto cursor = document.root_element();

  auto properties = cursor.element_properties();

  EXPECT_TRUE(properties.get_string(ElementProperty::WIDTH));
  EXPECT_EQ("8.2673in", properties.get_string(ElementProperty::WIDTH).value());
  EXPECT_TRUE(properties.get_string(ElementProperty::HEIGHT));
  EXPECT_EQ("11.6925in",
            properties.get_string(ElementProperty::HEIGHT).value());
  EXPECT_TRUE(properties.get_string(ElementProperty::MARGIN_TOP));
  EXPECT_EQ("0.7874in",
            properties.get_string(ElementProperty::MARGIN_TOP).value());
}

TEST(DocumentTest, odg) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odg/sample.odg"));

  EXPECT_EQ(document_file.file_type(), FileType::OPENDOCUMENT_GRAPHICS);

  Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::DRAWING);

  auto cursor = document.root_element();

  // TODO
  // EXPECT_EQ(drawing.page_count(), 3);

  cursor.for_each_child([](DocumentCursor &cursor, const std::uint32_t) {
    auto properties = cursor.element_properties();

    EXPECT_TRUE(properties.get_string(ElementProperty::WIDTH));
    EXPECT_EQ("21cm", properties.get_string(ElementProperty::WIDTH).value());
    EXPECT_TRUE(properties.get_string(ElementProperty::HEIGHT));
    EXPECT_EQ("29.7cm", properties.get_string(ElementProperty::HEIGHT).value());
    EXPECT_TRUE(properties.get_string(ElementProperty::MARGIN_TOP));
    EXPECT_EQ("1cm",
              properties.get_string(ElementProperty::MARGIN_TOP).value());
  });
}
