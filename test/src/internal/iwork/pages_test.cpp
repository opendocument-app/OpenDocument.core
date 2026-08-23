#include <odr/document.hpp>
#include <odr/document_element.hpp>
#include <odr/file.hpp>
#include <odr/logger.hpp>
#include <odr/odr.hpp>

#include <odr/internal/abstract/archive.hpp>
#include <odr/internal/abstract/filesystem.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/iwork/iwork_archive.hpp>
#include <odr/internal/zip/zip_file.hpp>

#include <test_util.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

using namespace odr;
using odr::test::TestData;

namespace {

/// The paragraphs of a text root, a line break reading as a newline.
std::vector<std::string> paragraphs(const Element root) {
  std::vector<std::string> result;

  for (const Element paragraph : root.children()) {
    EXPECT_EQ(paragraph.type(), ElementType::paragraph);

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
  // the anchor of a drawable is dropped: nothing reads drawables yet
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
TEST(Iwork, package_resolves_across_components) {
  using odr::internal::iwork::Package;

  const auto file =
      std::make_shared<odr::internal::DiskFile>(odr::internal::AbsPath(
          TestData::test_file_path("odr-public/pages/style-various-1.pages")));
  const auto filesystem =
      odr::internal::zip::ZipFile(file).archive()->as_filesystem();

  Package package(*filesystem);

  EXPECT_EQ(package.component("Document").objects().front().identifier, 1);
  // the stylesheet, which is a component of its own
  EXPECT_EQ(package.object(1732588).identifier, 1732588);
  // the root of a `Tables/DataList` that is not the first one of that name
  EXPECT_EQ(package.object(1732940).identifier, 1732940);
}
