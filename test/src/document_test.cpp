#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/html.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <filesystem>

using namespace odr;
using namespace odr::test;

TEST(Document, odt) {
  auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"), *logger);

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
  auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  DocumentFile document_file(
      TestData::test_file_path("odr-public/odg/sample.odg"), *logger);

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
  auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"), *logger);
  Document document = document_file.document();

  std::function<void(Element)> edit = [&](Element element) {
    for (Element child : element.children()) {
      edit(child);
    }
    // TODO make editing empty text possible
    if (auto text = element.as_text(); text && !text.content().empty()) {
      text.set_content("hello world!");
    }
  };
  edit(document.root_element());

  std::string output_path =
      (std::filesystem::current_path() / "about_edit.odt").string();
  document.save(output_path);

  DocumentFile validate_file(output_path);
  Document validate_document = validate_file.document();
  std::function<void(Element)> validate = [&](Element element) {
    for (Element child : element.children()) {
      validate(child);
    }
    // TODO make editing empty text possible
    if (auto text = element.as_text(); text && !text.content().empty()) {
      EXPECT_EQ("hello world!", text.content());
    }
  };
  validate(validate_document.root_element());
}

TEST(Document, edit_docx) {
  auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  DocumentFile document_file(
      TestData::test_file_path("odr-public/docx/style-various-1.docx"),
      *logger);
  Document document = document_file.document();

  std::function<void(Element)> edit = [&](Element element) {
    for (Element child : element.children()) {
      edit(child);
    }
    // TODO make editing empty text possible
    if (auto text = element.as_text(); text && !text.content().empty()) {
      text.set_content("hello world!");
    }
  };
  edit(document.root_element());

  std::string output_path =
      (std::filesystem::current_path() / "style-various-1_edit.docx").string();
  document.save(output_path);

  DocumentFile validate_file(output_path);
  Document validate_document = validate_file.document();
  std::function<void(Element)> validate = [&](Element element) {
    for (Element child : element.children()) {
      validate(child);
    }
    // TODO make editing empty text possible
    if (auto text = element.as_text(); text && !text.content().empty()) {
      EXPECT_EQ("hello world!", text.content());
    }
  };
  validate(validate_document.root_element());
}

TEST(Document, edit_odt_diff) {
  auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  auto diff =
      R"({"modifiedText":{"/child:16/child:0":"Outasdfsdafdline","/child:24/child:0":"Colorasdfasdfasdfed Line","/child:6/child:0":"Text hello world!"}})";
  DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/style-various-1.odt"), *logger);
  Document document = document_file.document();

  html::edit(document, diff);

  std::string output_path =
      (std::filesystem::current_path() / "style-various-1_edit_diff.odt")
          .string();
  document.save(output_path);

  DocumentFile validate_file(output_path);
  Document validate_document = validate_file.document();
  EXPECT_EQ("Outasdfsdafdline",
            DocumentPath::find(validate_document.root_element(),
                               DocumentPath("/child:16/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Colorasdfasdfasdfed Line",
            DocumentPath::find(validate_document.root_element(),
                               DocumentPath("/child:24/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Text hello world!",
            DocumentPath::find(validate_document.root_element(),
                               DocumentPath("/child:6/child:0"))
                .as_text()
                .content());
}

// TODO make sheet editing work by implementing table addressing
// TEST(Document, edit_ods_diff) {
//   auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);
//
//   auto diff =
//       R"({"modifiedText":{"/child:0/row:0/child:0/child:0/child:0":"Page 1
//       hi","/child:1/row:0/child:0/child:0/child:0":"Page 2
//       hihi","/child:2/row:0/child:0/child:0/child:0":"Page 3
//       hihihi","/child:3/row:0/child:0/child:0/child:0":"Page 4
//       hihihihi","/child:4/row:0/child:0/child:0/child:0":"Page 5
//       hihihihihi"}})";
//   DocumentFile document_file(
//       TestData::test_file_path("odr-public/ods/pages.ods"), *logger);
//   document_file = document_file.decrypt(
//       TestData::test_file("odr-public/ods/pages.ods").password.value());
//   Document document = document_file.document();
//
//   html::edit(document, diff);
//
//   std::string output_path =
//       (std::filesystem::current_path() / "pages_edit_diff.ods").string();
//   document.save(output_path);
//
//   DocumentFile validate_file(output_path);
//   Document validate_document = validate_file.document();
//   EXPECT_EQ(
//       "Page 1 hi",
//       DocumentPath::find(validate_document.root_element(),
//                          DocumentPath("/child:0/row:0/child:0/child:0/child:0"))
//           .as_text()
//           .content());
//   EXPECT_EQ(
//       "Page 2 hihi",
//       DocumentPath::find(validate_document.root_element(),
//                          DocumentPath("/child:1/row:0/child:0/child:0/child:0"))
//           .as_text()
//           .content());
//   EXPECT_EQ(
//       "Page 3 hihihi",
//       DocumentPath::find(validate_document.root_element(),
//                          DocumentPath("/child:2/row:0/child:0/child:0/child:0"))
//           .as_text()
//           .content());
//   EXPECT_EQ(
//       "Page 4 hihihihi",
//       DocumentPath::find(validate_document.root_element(),
//                          DocumentPath("/child:3/row:0/child:0/child:0/child:0"))
//           .as_text()
//           .content());
//   EXPECT_EQ(
//       "Page 5 hihihihihi",
//       DocumentPath::find(validate_document.root_element(),
//                          DocumentPath("/child:4/row:0/child:0/child:0/child:0"))
//           .as_text()
//           .content());
// }

TEST(Document, edit_docx_diff) {
  auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  auto diff =
      R"({"modifiedText":{"/child:16/child:0/child:0":"Outasdfsdafdline","/child:24/child:0/child:0":"Colorasdfasdfasdfed Line","/child:6/child:0/child:0":"Text hello world!"}})";
  DocumentFile document_file(
      TestData::test_file_path("odr-public/docx/style-various-1.docx"),
      *logger);
  Document document = document_file.document();

  html::edit(document, diff);

  std::string output_path =
      (std::filesystem::current_path() / "style-various-1_edit_diff.docx")
          .string();
  document.save(output_path);

  DocumentFile validate_file(output_path);
  Document validate_document = validate_file.document();
  EXPECT_EQ("Outasdfsdafdline",
            DocumentPath::find(validate_document.root_element(),
                               DocumentPath("/child:16/child:0/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Colorasdfasdfasdfed Line",
            DocumentPath::find(validate_document.root_element(),
                               DocumentPath("/child:24/child:0/child:0"))
                .as_text()
                .content());
  EXPECT_EQ("Text hello world!",
            DocumentPath::find(validate_document.root_element(),
                               DocumentPath("/child:6/child:0/child:0"))
                .as_text()
                .content());
}
