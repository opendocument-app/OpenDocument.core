#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/html.hpp>
#include <odr/quantity.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::test;

TEST(Document, odt) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"));

  EXPECT_EQ(document_file.file_type(), FileType::opendocument_text);

  Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::text);

  auto page_layout = document.root_element().as_text_root().page_layout();
  EXPECT_TRUE(page_layout.width);
  EXPECT_EQ(Measure("8.2673in"), page_layout.width);
  EXPECT_TRUE(page_layout.height);
  EXPECT_EQ(Measure("11.6925in"), page_layout.height);
  EXPECT_TRUE(page_layout.margin.top);
  EXPECT_EQ(Measure("0.7874in"), page_layout.margin.top);
}

TEST(Document, odg) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odg/sample.odg"));

  EXPECT_EQ(document_file.file_type(), FileType::opendocument_graphics);

  Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::drawing);

  for (auto child : document.root_element().children()) {
    auto page_layout = child.as_page().page_layout();
    EXPECT_TRUE(page_layout.width);
    EXPECT_EQ(Measure("21cm"), page_layout.width);
    EXPECT_TRUE(page_layout.height);
    EXPECT_EQ(Measure("29.7cm"), page_layout.height);
    EXPECT_TRUE(page_layout.margin.top);
    EXPECT_EQ(Measure("1cm"), page_layout.margin.top);
  }
}

TEST(Document, edit_odt) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"));
  Document document = document_file.document();

  std::function<void(Element)> edit = [&](Element element) {
    for (Element child : element.children()) {
      edit(child);
    }
    if (auto text = element.as_text()) {
      text.set_content("hello world!");
    }
  };
  edit(document.root_element());

  document.save("about_edit.odt");
  DocumentFile("about_edit.odt");
}

TEST(Document, edit_docx) {
  DocumentFile document_file(
      TestData::test_file_path("odr-public/docx/style-various-1.docx"));
  Document document = document_file.document();

  std::function<void(Element)> edit = [&](Element element) {
    for (Element child : element.children()) {
      edit(child);
    }
    if (auto text = element.as_text()) {
      text.set_content("hello world!");
    }
  };
  edit(document.root_element());

  document.save("style-various-1_edit.docx");
  DocumentFile("style-various-1_edit.docx");
}

TEST(Document, edit_odt_diff) {
  auto diff =
      R"({"modifiedText":{"/child:16/child:0":"Outasdfsdafdline","/child:24/child:0":"Colorasdfasdfasdfed Line","/child:6/child:0":"Text hello world!"}})";
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/style-various-1.odt"));
  Document document = document_file.document();

  html::edit(document, diff);

  document.save("style-various-1_edit_diff.odt");
  DocumentFile("style-various-1_edit_diff.odt");
}

TEST(Document, edit_ods_diff) {
  auto diff =
      R"({"modifiedText":{"/child:0/row:0/child:0/child:0/child:0":"Page 1 hi","/child:1/row:0/child:0/child:0/child:0":"Page 2 hihi","/child:2/row:0/child:0/child:0/child:0":"Page 3 hihihi","/child:3/row:0/child:0/child:0/child:0":"Page 4 hihihihi","/child:4/row:0/child:0/child:0/child:0":"Page 5 hihihihihi"}})";
  DocumentFile document_file(
      TestData::test_file_path("odr-public/ods/pages.ods"));
  document_file = document_file.decrypt(
      TestData::test_file("odr-public/ods/pages.ods").password.value());
  Document document = document_file.document();

  html::edit(document, diff);

  document.save("pages_edit_diff.ods");
  DocumentFile("pages_edit_diff.ods");
}

TEST(Document, edit_docx_diff) {
  auto diff =
      R"({"modifiedText":{"/child:16/child:0/child:0":"Outasdfsdafdline","/child:24/child:0/child:0":"Colorasdfasdfasdfed Line","/child:6/child:0/child:0":"Text hello world!"}})";
  DocumentFile document_file(
      TestData::test_file_path("odr-public/docx/style-various-1.docx"));
  Document document = document_file.document();

  html::edit(document, diff);

  document.save("style-various-1_edit_diff.docx");
  DocumentFile("style-various-1_edit_diff.docx");
}
