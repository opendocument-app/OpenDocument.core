#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/odr.hpp>
#include <odr/style.hpp>

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <tuple>
#include <vector>

using namespace odr;

namespace {

std::vector<Element> children(const Element &element) {
  std::vector<Element> result;
  for (const Element child : element.children()) {
    result.push_back(child);
  }
  return result;
}

Document reopen(const Document &document) {
  return open(document.save_to_memory(),
              DecodeOptions::as(document.file_type()))
      .as_document_file()
      .document();
}

} // namespace

TEST(DocumentCreate, refuses_a_type_without_the_capability) {
  for (const FileType type : all_file_types()) {
    if (capabilities_by_file_type(type).create) {
      continue;
    }
    EXPECT_THROW(std::ignore = create_document(type), UnsupportedFileType)
        << file_type_to_string(type);
  }
}

TEST(DocumentCreate, makes_every_type_with_the_capability) {
  for (const FileType type : all_file_types()) {
    if (!capabilities_by_file_type(type).create) {
      continue;
    }
    const Document document = create_document(type);
    EXPECT_EQ(document.file_type(), type) << file_type_to_string(type);
    EXPECT_EQ(document.document_type(), document_type_by_file_type(type))
        << file_type_to_string(type);
    EXPECT_TRUE(document.is_editable()) << file_type_to_string(type);
    EXPECT_TRUE(document.is_savable()) << file_type_to_string(type);
  }
}

TEST(DocumentCreate, odt_holds_one_empty_paragraph) {
  const Document document = create_document(FileType::opendocument_text);

  const std::vector<Element> body = children(document.root_element());
  ASSERT_EQ(body.size(), 1);
  ASSERT_EQ(body[0].type(), ElementType::paragraph);
  EXPECT_TRUE(children(body[0]).empty());

  const TextStyle style = body[0].as_paragraph().text_style();
  ASSERT_TRUE(style.font_name.has_value());
  EXPECT_NE(style.font_name->find("Liberation Serif"), std::string::npos);
  EXPECT_EQ(style.font_size->to_string(), "12pt");
}

TEST(DocumentCreate, odt_page_is_a4) {
  const Document document = create_document(FileType::opendocument_text);

  const PageLayout layout =
      document.root_element().as_text_root().page_layout();
  EXPECT_EQ(layout.width->to_string(), "21cm");
  EXPECT_EQ(layout.height->to_string(), "29.7cm");
  EXPECT_EQ(layout.margin.left->to_string(), "2cm");
}

TEST(DocumentCreate, odt_takes_an_edit_and_keeps_it_through_a_save) {
  const Document document = create_document(FileType::opendocument_text);
  const Paragraph first =
      (*document.root_element().children().begin()).as_paragraph();

  std::ignore = document.append_text(first, "hello");
  const Paragraph second = document.insert_paragraph_after(first);
  std::ignore = document.append_text(second, "world");

  const Document saved = reopen(document);
  const std::vector<Element> body = children(saved.root_element());
  ASSERT_EQ(body.size(), 2);
  EXPECT_EQ(children(body[0]).at(0).as_text().content(), "hello");
  EXPECT_EQ(children(body[1]).at(0).as_text().content(), "world");
}

TEST(DocumentCreate, is_the_same_bytes_on_every_call) {
  for (const FileType type : all_file_types()) {
    if (!capabilities_by_file_type(type).create) {
      continue;
    }
    std::ostringstream first;
    std::ostringstream second;
    create_document(type).save(first);
    create_document(type).save(second);
    EXPECT_EQ(first.str(), second.str()) << file_type_to_string(type);
  }
}
