#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/html.hpp>
#include <odr/style.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <filesystem>

using namespace odr;
using namespace odr::test;

TEST(Document, odt) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"), *logger);

  EXPECT_EQ(document_file.file_type(), FileType::opendocument_text);

  const Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::text);

  const PageLayout page_layout =
      document.root_element().as_text_root().page_layout();
  EXPECT_TRUE(page_layout.width.has_value());
  EXPECT_EQ(Measure("8.2673in"), page_layout.width);
  EXPECT_TRUE(page_layout.height.has_value());
  EXPECT_EQ(Measure("11.6925in"), page_layout.height);
  EXPECT_TRUE(page_layout.margin.top.has_value());
  EXPECT_EQ(Measure("0.7874in"), page_layout.margin.top);
}

TEST(Document, odt_element_path) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"), *logger);

  EXPECT_EQ(document_file.file_type(), FileType::opendocument_text);

  const Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::text);

  const Element root = document.root_element();
  const Element p1 = root.first_child();
  EXPECT_EQ(p1.type(), ElementType::paragraph);
  const Element t1 = p1.first_child();
  EXPECT_EQ(t1.type(), ElementType::text);

  const DocumentPath t1_path = t1.document_path();
  const Element t1_via_path = root.navigate_path(t1_path);
  EXPECT_EQ(t1_via_path, t1);
}

TEST(Document, odt_element_path2) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/style-various-1.odt"), *logger);

  EXPECT_EQ(document_file.file_type(), FileType::opendocument_text);

  const Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::text);

  const Element root = document.root_element();

  const Element cell_via_path = root.navigate_path(
      DocumentPath("/child:41/child:0/child:1/child:0/child:0"));
  EXPECT_EQ(cell_via_path.type(), ElementType::text);
  EXPECT_EQ(cell_via_path.as_text().content(), "B1");
}

TEST(Document, odg) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odg/sample.odg"), *logger);

  EXPECT_EQ(document_file.file_type(), FileType::opendocument_graphics);

  const Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::drawing);

  for (const Element child : document.root_element().children()) {
    const PageLayout page_layout = child.as_page().page_layout();
    EXPECT_TRUE(page_layout.width.has_value());
    EXPECT_EQ(Measure("21cm"), page_layout.width);
    EXPECT_TRUE(page_layout.height.has_value());
    EXPECT_EQ(Measure("29.7cm"), page_layout.height);
    EXPECT_TRUE(page_layout.margin.top.has_value());
    EXPECT_EQ(Measure("1cm"), page_layout.margin.top);
  }
}

TEST(Document, edit_odt) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"), *logger);
  const Document document = document_file.document();

  std::function<void(Element)> edit = [&](const Element &element) {
    for (const Element child : element.children()) {
      edit(child);
    }
    // TODO make editing empty text possible
    if (const Text text = element.as_text(); text && !text.content().empty()) {
      text.set_content("hello world!");
    }
  };
  edit(document.root_element());

  const std::string output_path =
      (std::filesystem::current_path() / "about_edit.odt").string();
  document.save(output_path);

  const DocumentFile validate_file(output_path);
  const Document validate_document = validate_file.document();
  std::function<void(Element)> validate = [&](const Element &element) {
    for (const Element child : element.children()) {
      validate(child);
    }
    // TODO make editing empty text possible
    if (const Text text = element.as_text(); text && !text.content().empty()) {
      EXPECT_EQ("hello world!", text.content());
    }
  };
  validate(validate_document.root_element());
}

TEST(Document, edit_docx) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/docx/style-various-1.docx"),
      *logger);
  const Document document = document_file.document();

  std::function<void(Element)> edit = [&](const Element &element) {
    for (const Element child : element.children()) {
      edit(child);
    }
    // TODO make editing empty text possible
    if (const Text text = element.as_text(); text && !text.content().empty()) {
      text.set_content("hello world!");
    }
  };
  edit(document.root_element());

  const std::string output_path =
      (std::filesystem::current_path() / "style-various-1_edit.docx").string();
  document.save(output_path);

  const DocumentFile validate_file(output_path);
  const Document validate_document = validate_file.document();
  std::function<void(Element)> validate = [&](const Element &element) {
    for (const Element child : element.children()) {
      validate(child);
    }
    // TODO make editing empty text possible
    if (const Text text = element.as_text(); text && !text.content().empty()) {
      EXPECT_EQ("hello world!", text.content());
    }
  };
  validate(validate_document.root_element());
}

TEST(Document, edit_odt_diff) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const char *diff =
      R"({"modifiedText":{"/child:16/child:0":"Outasdfsdafdline","/child:24/child:0":"Colorasdfasdfasdfed Line","/child:6/child:0":"Text hello world!"}})";
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/style-various-1.odt"), *logger);
  const Document document = document_file.document();

  html::edit(document, diff);

  const std::string output_path =
      (std::filesystem::current_path() / "style-various-1_edit_diff.odt")
          .string();
  document.save(output_path);

  const DocumentFile validate_file(output_path);
  const Document validate_document = validate_file.document();
  EXPECT_EQ("Outasdfsdafdline",
            validate_document.root_element()
                .navigate_path(DocumentPath("/child:16/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Colorasdfasdfasdfed Line",
            validate_document.root_element()
                .navigate_path(DocumentPath("/child:24/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Text hello world!",
            validate_document.root_element()
                .navigate_path(DocumentPath("/child:6/child:0"))
                .as_text()
                .content());
}

TEST(Document, edit_ods_diff) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const char *diff =
      R"({"modifiedText":{"/child:0/cell:A1/child:0/child:0":"Page 1 hi","/child:1/cell:A1/child:0/child:0":"Page 2 hihi","/child:2/cell:A1/child:0/child:0":"Page 3 hihihi","/child:3/cell:A1/child:0/child:0":"Page 4 hihihihi","/child:4/cell:A1/child:0/child:0":"Page 5 hihihihihi"}})";
  DocumentFile document_file(
      TestData::test_file_path("odr-public/ods/pages.ods"), *logger);
  document_file = document_file.decrypt(
      TestData::test_file("odr-public/ods/pages.ods").password.value());
  const Document document = document_file.document();

  html::edit(document, diff);

  const std::string output_path =
      (std::filesystem::current_path() / "pages_edit_diff.ods").string();
  document.save(output_path);

  const DocumentFile validate_file(output_path);
  const Document validate_document = validate_file.document();
  EXPECT_EQ("Page 1 hi",
            validate_document.root_element()
                .navigate_path(DocumentPath("/child:0/cell:A1/child:0/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Page 2 hihi",
            validate_document.root_element()
                .navigate_path(DocumentPath("/child:1/cell:A1/child:0/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Page 3 hihihi",
            validate_document.root_element()
                .navigate_path(DocumentPath("/child:2/cell:A1/child:0/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Page 4 hihihihi",
            validate_document.root_element()
                .navigate_path(DocumentPath("/child:3/cell:A1/child:0/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Page 5 hihihihihi",
            validate_document.root_element()
                .navigate_path(DocumentPath("/child:4/cell:A1/child:0/child:0"))
                .as_text()
                .content());
}

TEST(Document, edit_docx_diff) {
  const std::unique_ptr logger =
      Logger::create_stdio("odr-test", LogLevel::verbose);

  const auto *diff =
      R"({"modifiedText":{"/child:16/child:0/child:0":"Outasdfsdafdline","/child:24/child:0/child:0":"Colorasdfasdfasdfed Line","/child:6/child:0/child:0":"Text hello world!"}})";
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/docx/style-various-1.docx"),
      *logger);
  const Document document = document_file.document();

  html::edit(document, diff);

  const std::string output_path =
      (std::filesystem::current_path() / "style-various-1_edit_diff.docx")
          .string();
  document.save(output_path);

  const DocumentFile validate_file(output_path);
  const Document validate_document = validate_file.document();
  EXPECT_EQ("Outasdfsdafdline",
            validate_document.root_element()
                .navigate_path(DocumentPath("/child:16/child:0/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Colorasdfasdfasdfed Line",
            validate_document.root_element()
                .navigate_path(DocumentPath("/child:24/child:0/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Text hello world!",
            validate_document.root_element()
                .navigate_path(DocumentPath("/child:6/child:0/child:0"))
                .as_text()
                .content());
}
