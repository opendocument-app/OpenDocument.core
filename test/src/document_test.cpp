#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/document_path.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/html.hpp>
#include <odr/style.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <optional>
#include <sstream>
#include <string>

using namespace odr;
using namespace odr::test;

namespace {

Element find_paragraph_with_text_prefix(const Element root,
                                        const std::string &prefix) {
  for (const Element child : root.children()) {
    if (child.type() == ElementType::paragraph) {
      if (const Element first_child = child.first_child();
          first_child && first_child.type() == ElementType::text &&
          first_child.as_text().content().starts_with(prefix)) {
        return child;
      }
    }
    if (const Element match = find_paragraph_with_text_prefix(child, prefix)) {
      return match;
    }
  }
  return {};
}

// Both skip empty texts — TODO make editing empty text possible.

void set_every_text(const Element element, const std::string &content) {
  for (const Element child : element.children()) {
    set_every_text(child, content);
  }
  if (const Text text = element.as_text(); text && !text.content().empty()) {
    text.set_content(content);
  }
}

void expect_every_text(const Element element, const std::string &content) {
  for (const Element child : element.children()) {
    expect_every_text(child, content);
  }
  if (const Text text = element.as_text(); text && !text.content().empty()) {
    EXPECT_EQ(content, text.content());
  }
}

void expect_text_at(const Document &document, const std::string &path,
                    const std::string &expected) {
  EXPECT_EQ(expected, document.root_element()
                          .navigate_path(DocumentPath(path))
                          .as_text()
                          .content());
}

/// Applies `diff` to `path`'s document, saves to `output_name` in the working
/// directory and reopens it, so the assertions see what was written.
Document edit_and_reload(const std::string &path, const char *diff,
                         const std::string &output_name) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(TestData::test_file_path(path), logger);
  const Document document = document_file.document();

  html::edit(document, diff);

  const std::string output_path =
      (std::filesystem::current_path() / output_name).string();
  document.save(output_path);

  return DocumentFile(output_path).document();
}

/// `pages.ods` is password-protected; every test that wants its content opens
/// it this way.
Document decrypted_pages_ods() {
  const std::string path = "odr-public/ods/pages.ods";
  return DocumentFile(TestData::test_file_path(path))
      .decrypt(TestData::test_file(path).password.value())
      .document();
}

} // namespace

TEST(Document, odt) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"), logger);

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

TEST(Document, docx_page_layout) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/docx/sample3.docx"), logger);

  const Document document = document_file.document();

  const PageLayout page_layout =
      document.root_element().as_text_root().page_layout();
  // Two sections; the 0.5in bottom margin is the first one's, the body's own
  // `w:sectPr` says 1in.
  EXPECT_EQ(Measure("8.5in"), page_layout.width);
  EXPECT_EQ(Measure("11in"), page_layout.height);
  EXPECT_EQ(Measure("1in"), page_layout.margin.top);
  EXPECT_EQ(Measure("1in"), page_layout.margin.right);
  EXPECT_EQ(Measure("0.5in"), page_layout.margin.bottom);
  EXPECT_EQ(Measure("1in"), page_layout.margin.left);
}

// #816
TEST(Document, ods_sheet_page_layout) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/ods/file_example_ODS_100.ods"),
      logger);

  const Document document = document_file.document();
  const PageLayout page_layout =
      (*document.root_element().children().begin()).as_sheet().page_layout();
  EXPECT_EQ(Measure("8.2681in"), page_layout.width);
  EXPECT_EQ(Measure("11.6929in"), page_layout.height);
  EXPECT_EQ(PrintOrientation::portrait, page_layout.print_orientation);
  EXPECT_EQ(Measure("0.7874in"), page_layout.margin.top);
  EXPECT_EQ(Measure("0.7874in"), page_layout.margin.right);
  EXPECT_EQ(Measure("0.7874in"), page_layout.margin.bottom);
  EXPECT_EQ(Measure("0.7874in"), page_layout.margin.left);
}

// Producers commonly state margins only and leave the paper to the printer.
TEST(Document, ods_sheet_page_layout_without_a_paper_size) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/ods/file_example_ODS_10.ods"),
      logger);

  const Document document = document_file.document();
  const PageLayout page_layout =
      (*document.root_element().children().begin()).as_sheet().page_layout();

  EXPECT_FALSE(page_layout.width.has_value());
  EXPECT_FALSE(page_layout.height.has_value());
  EXPECT_EQ(Measure("0.7874in"), page_layout.margin.left);
}

TEST(Document, xlsx_sheet_names) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/xlsx/sampledatainsurance.xlsx"),
      logger);
  const Document document = document_file.document();

  std::vector<std::string> names;
  for (const Element child : document.root_element().children()) {
    names.push_back(child.as_sheet().name());
  }

  EXPECT_EQ((std::vector<std::string>{"Instructions", "PolicyData", "MyLinks"}),
            names);
}

TEST(Document, odt_element_path) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/about.odt"), logger);

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
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/style-various-1.odt"), logger);

  EXPECT_EQ(document_file.file_type(), FileType::opendocument_text);

  const Document document = document_file.document();

  EXPECT_EQ(document.document_type(), DocumentType::text);

  const Element root = document.root_element();

  const Element cell_via_path = root.navigate_path(
      DocumentPath("/child:41/child:0/child:1/child:0/child:0"));
  EXPECT_EQ(cell_via_path.type(), ElementType::text);
  EXPECT_EQ(cell_via_path.as_text().content(), "B1");
}

TEST(Document, odt_text_position) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/style-various-1.odt"), logger);
  const Document document = document_file.document();
  const Element root = document.root_element();

  const Element superscript =
      find_paragraph_with_text_prefix(root, "Superscript");
  ASSERT_TRUE(superscript);
  const TextStyle superscript_style = superscript.as_paragraph().text_style();
  EXPECT_EQ(FontPosition::super, superscript_style.font_position);
  // `style:text-position="super 58%"` scales the inherited 12pt font
  ASSERT_TRUE(superscript_style.font_size.has_value());
  EXPECT_NEAR(6.96, superscript_style.font_size->magnitude(), 1e-9);

  const Element subscript = find_paragraph_with_text_prefix(root, "Subscript");
  ASSERT_TRUE(subscript);
  const TextStyle subscript_style = subscript.as_paragraph().text_style();
  EXPECT_EQ(FontPosition::sub, subscript_style.font_position);

  const Element normal = find_paragraph_with_text_prefix(root, "Default");
  ASSERT_TRUE(normal);
  EXPECT_FALSE(normal.as_paragraph().text_style().font_position.has_value());
}

TEST(Document, odt_line_height_and_text_indent) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odt/file-sample_100kB.odt"), logger);
  const Document document = document_file.document();
  const Element root = document.root_element();

  const Element heading =
      find_paragraph_with_text_prefix(root, "Lorem ipsum dolor");
  ASSERT_TRUE(heading);
  EXPECT_EQ(Measure("0.25in"), heading.as_paragraph().style().text_indent);

  const Element body = find_paragraph_with_text_prefix(root, "Morbi viverra");
  ASSERT_TRUE(body);
  EXPECT_EQ(Measure("120%"), body.as_paragraph().style().line_height);
}

TEST(Document, odg) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/odg/sample.odg"), logger);

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

namespace {

void edit_every_text_and_reload(const std::string &path,
                                const std::string &output_name) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(TestData::test_file_path(path), logger);
  const Document document = document_file.document();

  set_every_text(document.root_element(), "hello world!");

  const std::string output_path =
      (std::filesystem::current_path() / output_name).string();
  document.save(output_path);

  const Document reloaded = DocumentFile(output_path).document();
  expect_every_text(reloaded.root_element(), "hello world!");
}

} // namespace

TEST(Document, edit_odt) {
  edit_every_text_and_reload("odr-public/odt/about.odt", "about_edit.odt");
}

TEST(Document, edit_docx) {
  edit_every_text_and_reload("odr-public/docx/style-various-1.docx",
                             "style-various-1_edit.docx");
}

TEST(Document, edit_odt_diff) {
  const char *diff =
      R"({"modifiedText":{"/child:16/child:0":"Outasdfsdafdline","/child:24/child:0":"Colorasdfasdfasdfed Line","/child:6/child:0":"Text hello world!"}})";
  const Document document =
      edit_and_reload("odr-public/odt/style-various-1.odt", diff,
                      "style-various-1_edit_diff.odt");

  expect_text_at(document, "/child:16/child:0", "Outasdfsdafdline");
  expect_text_at(document, "/child:24/child:0", "Colorasdfasdfasdfed Line");
  expect_text_at(document, "/child:6/child:0", "Text hello world!");
}

// Asserted in memory: `pages.ods` is password-protected, and a decrypted
// package is not savable — see `a_decrypted_package_is_not_savable`.
TEST(Document, edit_ods_diff) {
  const char *diff =
      R"({"modifiedText":{"/child:0/cell:A1/child:0/child:0":"Page 1 hi","/child:1/cell:A1/child:0/child:0":"Page 2 hihi","/child:2/cell:A1/child:0/child:0":"Page 3 hihihi","/child:3/cell:A1/child:0/child:0":"Page 4 hihihihi","/child:4/cell:A1/child:0/child:0":"Page 5 hihihihihi"}})";
  const Document document = decrypted_pages_ods();

  html::edit(document, diff);

  expect_text_at(document, "/child:0/cell:A1/child:0/child:0", "Page 1 hi");
  expect_text_at(document, "/child:1/cell:A1/child:0/child:0", "Page 2 hihi");
  expect_text_at(document, "/child:2/cell:A1/child:0/child:0", "Page 3 hihihi");
  expect_text_at(document, "/child:3/cell:A1/child:0/child:0",
                 "Page 4 hihihihi");
  expect_text_at(document, "/child:4/cell:A1/child:0/child:0",
                 "Page 5 hihihihihi");
}

// Saving would rebuild the package from the decrypted filesystem, handing out
// what the password protects.
TEST(Document, a_decrypted_package_is_not_savable) {
  const Document document = decrypted_pages_ods();

  EXPECT_FALSE(document.is_savable());
  EXPECT_FALSE(document.is_savable(true));

  const std::string path =
      (std::filesystem::current_path() / "decrypted_save.ods").string();
  EXPECT_THROW(document.save(path), UnsupportedOperation);
  EXPECT_FALSE(std::filesystem::exists(path));

  std::ostringstream out;
  EXPECT_THROW(document.save(out), UnsupportedOperation);
  EXPECT_THROW((void)document.save_to_memory(), UnsupportedOperation);
}

TEST(Document, edit_docx_diff) {
  const char *diff =
      R"({"modifiedText":{"/child:16/child:0/child:0":"Outasdfsdafdline","/child:24/child:0/child:0":"Colorasdfasdfasdfed Line","/child:6/child:0/child:0":"Text hello world!"}})";
  const Document document =
      edit_and_reload("odr-public/docx/style-various-1.docx", diff,
                      "style-various-1_edit_diff.docx");

  expect_text_at(document, "/child:16/child:0/child:0", "Outasdfsdafdline");
  expect_text_at(document, "/child:24/child:0/child:0",
                 "Colorasdfasdfasdfed Line");
  expect_text_at(document, "/child:6/child:0/child:0", "Text hello world!");
}

namespace {

/// The one document `save` writes as xml rather than as a zip.
constexpr const char *flat_odt =
    R"(<?xml version="1.0" encoding="UTF-8"?>)"
    R"(<office:document office:mimetype=")"
    R"(application/vnd.oasis.opendocument.text">)"
    R"(<office:body><office:text><text:p>hello</text:p></office:text>)"
    R"(</office:body></office:document>)";

} // namespace

TEST(Document, save_to_memory_round_trips_a_flat_document) {
  const Document document = DocumentFile::from_memory(flat_odt).document();

  set_every_text(document.root_element(), "hello world!");

  const File saved = document.save_to_memory();
  EXPECT_EQ(FileLocation::memory, saved.location());

  const Document reloaded = DocumentFile(saved).document();
  expect_every_text(reloaded.root_element(), "hello world!");
}

TEST(Document, save_to_a_stream_writes_what_save_to_memory_holds) {
  const Document document = DocumentFile::from_memory(flat_odt).document();

  std::ostringstream out;
  document.save(out);

  EXPECT_EQ(out.str(), document.save_to_memory().memory_data().value());
}

TEST(Document, save_to_memory_round_trips_a_package) {
  const Document document =
      DocumentFile(TestData::test_file_path("odr-public/odt/about.odt"))
          .document();

  set_every_text(document.root_element(), "hello world!");

  const Document reloaded = DocumentFile(document.save_to_memory()).document();
  expect_every_text(reloaded.root_element(), "hello world!");
}

TEST(Document, saving_an_unsavable_format_leaves_no_file) {
  const Document document =
      DocumentFile(
          TestData::test_file_path("odr-public/pptx/style-various-1.pptx"))
          .document();
  ASSERT_FALSE(document.is_savable());

  const std::string path =
      (std::filesystem::current_path() / "unsavable_save.pptx").string();
  EXPECT_THROW(document.save(path), UnsupportedOperation);
  EXPECT_FALSE(std::filesystem::exists(path));

  std::ostringstream out;
  EXPECT_THROW(document.save(out), UnsupportedOperation);
  EXPECT_THROW((void)document.save_to_memory(), UnsupportedOperation);
}
