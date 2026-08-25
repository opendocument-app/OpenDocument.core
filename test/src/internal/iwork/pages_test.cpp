#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/exceptions.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>
#include <odr/table_dimension.hpp>

#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/iwork/iwork_archive.hpp>
#include <odr/internal/iwork/iwork_document.hpp>
#include <odr/internal/iwork/iwork_file.hpp>
#include <odr/internal/zip/zip_file.hpp>

#include <internal/iwork/iwork_test_util.hpp>
#include <test_util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

using namespace odr;
using odr::test::TestData;
namespace builder = odr::test::iwork;
namespace iwork = odr::internal::iwork;

namespace {

/// The paragraphs of a text root, a line break reading as a newline. A body
/// also holds the tables its text anchors, which this passes over.
std::vector<std::string> paragraphs(const Element root) {
  std::vector<std::string> result;

  for (const Element paragraph : root.children()) {
    if (paragraph.type() != ElementType::paragraph) {
      continue;
    }

    std::string text;
    for (const Element child : paragraph.children()) {
      if (child.type() == ElementType::line_break) {
        text += '\n';
      } else {
        text += child.as_text().content();
      }
    }
    result.push_back(std::move(text));
  }

  return result;
}

/// The document a synthetic package decodes to.
Document pages_document(
    const std::string &text,
    const std::optional<std::vector<std::uint64_t>> &paragraph_indices) {
  return Document(std::make_shared<iwork::Document>(
      FileType::iwork_pages,
      builder::pages_package(builder::text_storage(text, paragraph_indices))));
}

} // namespace

TEST(Iwork, pages_is_detected_by_content) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);
  const std::string path =
      TestData::test_file_path("odr-public/pages/style-various-1.pages");

  EXPECT_THAT(list_file_types(path, logger),
              testing::Contains(FileType::iwork_pages));

  const DecodedFile file(path, logger);
  EXPECT_EQ(file.file_type(), FileType::iwork_pages);
  EXPECT_EQ(file.file_category(), FileCategory::document);
  EXPECT_EQ(file.as_document_file().document_type(), DocumentType::text);
}

// A document with nothing in it must come back with an empty body rather than
// throw: `empty.pages` carries a body storage that holds no text at all.
TEST(Iwork, pages_empty) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/pages/empty.pages"), logger);
  EXPECT_EQ(document_file.file_type(), FileType::iwork_pages);

  const Document document = document_file.document();
  EXPECT_EQ(document.document_type(), DocumentType::text);
  EXPECT_FALSE(document.is_editable());
  EXPECT_FALSE(document.is_savable(false));

  EXPECT_EQ(paragraphs(document.root_element()),
            (std::vector<std::string>{""}));
}

TEST(Iwork, pages_body_text) {
  const Logger logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const DocumentFile document_file(
      TestData::test_file_path("odr-public/pages/style-various-1.pages"),
      logger);

  const Document document = document_file.document();
  const std::vector<std::string> text = paragraphs(document.root_element());

  // one element per paragraph of the body, including the empty ones that
  // separate its sections
  ASSERT_EQ(text.size(), 54);
  EXPECT_EQ(text[0], "Table of Contents");
  // the anchor of a drawable is dropped from the text; a table anchored there
  // becomes an element of its own, and an image is still not read
  EXPECT_EQ(text[1], "");
  EXPECT_EQ(text[4], "Headline");
  EXPECT_EQ(text[5], "Nested Headline");
  EXPECT_EQ(text[7], "Text");
  EXPECT_EQ(text[9], "Hyperlink google");
  EXPECT_EQ(text[11], "Default");
  EXPECT_EQ(text[12], "Bold");
  EXPECT_EQ(text.back(), "image");
}

// Components share names — `style-various-1.pages` holds two dozen called
// `Tables/DataList` — so the package has to load them by locator. Keying on
// the name hands back the wrong file and leaves the rest never loaded, which
// shows up as an object nothing can resolve.
// A `U+FFFC` in the body anchors a drawable, which the attachment run table
// names. The one that is a table becomes a `Table` after the paragraph its
// anchor sits in; its cells hold rich text, one storage each.
TEST(Iwork, pages_table) {
  const DocumentFile document_file(
      TestData::test_file_path("odr-public/pages/style-various-1.pages"),
      Logger::null());
  const Document document = document_file.document();

  std::vector<Element> tables;
  for (const Element child : document.root_element().children()) {
    if (child.type() == ElementType::table) {
      tables.push_back(child);
    }
  }
  ASSERT_EQ(tables.size(), 1);

  const Table table = tables.front().as_table();
  EXPECT_EQ(table.dimensions().rows, 2);
  EXPECT_EQ(table.dimensions().columns, 2);

  std::vector<std::vector<std::string>> cells;
  for (const Element row : table.rows()) {
    std::vector<std::string> texts;
    for (const Element cell : row.children()) {
      std::string text;
      for (const Element paragraph : cell.children()) {
        if (!text.empty()) {
          text += '\n';
        }
        for (const Element child : paragraph.children()) {
          if (child.type() != ElementType::line_break) {
            text += child.as_text().content();
          }
        }
      }
      texts.push_back(std::move(text));
    }
    cells.push_back(std::move(texts));
  }

  EXPECT_EQ(cells, (std::vector<std::vector<std::string>>{
                       {"A1", "B1\nasdf"},
                       {"A2", "B2\n\n"},
                   }));
}

TEST(Iwork, package_resolves_across_components) {
  const auto file = std::make_shared<internal::DiskFile>(internal::AbsPath(
      TestData::test_file_path("odr-public/pages/style-various-1.pages")));
  const auto filesystem =
      internal::zip::ZipFile(file).archive()->as_filesystem();

  iwork::Package package(*filesystem);

  EXPECT_EQ(package.component("Document").objects().front().identifier, 1);
  // the stylesheet, which is a component of its own
  EXPECT_EQ(package.object(1732588).identifier, 1732588);
  // the root of a `Tables/DataList` that is not the first one of that name
  EXPECT_EQ(package.object(1732940).identifier, 1732940);
}

// Run tables count UTF-16 code units over UTF-8 text: a character outside the
// basic multilingual plane is two units but four bytes.
TEST(Iwork, pages_paragraph_starts_after_a_surrogate_pair) {
  const Document document =
      pages_document("a\xf0\x9f\x98\x80\nbcd\n", {{0, 4}});

  EXPECT_EQ(paragraphs(document.root_element()),
            (std::vector<std::string>{"a\xf0\x9f\x98\x80", "bcd"}));
}

TEST(Iwork, pages_run_table_points_past_the_text) {
  EXPECT_ANY_THROW(std::ignore = pages_document("abc", {{0, 9}}));
}

TEST(Iwork, pages_text_ends_mid_character) {
  EXPECT_ANY_THROW(std::ignore = pages_document("a\xe2\x80", {{0, 3}}));
}

TEST(Iwork, pages_text_is_not_utf8) {
  EXPECT_ANY_THROW(std::ignore = pages_document("\x80x", {{0, 1}}));
}

// `U+2028` breaks a line inside a paragraph rather than starting a new one.
TEST(Iwork, pages_line_separator_breaks_a_line_inside_a_paragraph) {
  const Document document = pages_document("one\xe2\x80\xa8two\n", {{0}});

  const Element root = document.root_element();
  ASSERT_EQ(paragraphs(root), (std::vector<std::string>{"one\ntwo"}));

  std::vector<ElementType> types;
  for (const Element child : (*root.children().begin()).children()) {
    types.push_back(child.type());
  }
  EXPECT_EQ(types, (std::vector<ElementType>{ElementType::text,
                                             ElementType::line_break,
                                             ElementType::text}));
}

TEST(Iwork, pages_without_a_paragraph_style_table) {
  const Document document = pages_document("only\n", std::nullopt);

  EXPECT_EQ(paragraphs(document.root_element()),
            (std::vector<std::string>{"only"}));
}

TEST(Iwork, pages_with_an_empty_paragraph_style_table) {
  const Document document =
      pages_document("only\n", std::vector<std::uint64_t>{});

  EXPECT_EQ(paragraphs(document.root_element()),
            (std::vector<std::string>{"only"}));
}

// A root archive type nothing maps falls back to the zip the package is,
// rather than being guessed at from the extension.
TEST(Iwork, unmapped_root_archive_is_not_an_iwork_file) {
  const auto files =
      builder::pages_package(builder::text_storage("", std::nullopt), 10001);

  EXPECT_THROW(iwork::IworkFile{files}, NoIworkFile);
}

TEST(Iwork, package_without_a_document_component_is_not_an_iwork_file) {
  const auto files = builder::filesystem({});

  EXPECT_THROW(iwork::IworkFile{files}, NoIworkFile);
}
