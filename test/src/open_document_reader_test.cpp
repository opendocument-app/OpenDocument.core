#include <gtest/gtest.h>
#include <odr/file.h>
#include <odr/open_document_reader.h>
#include <test_util.h>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;

TEST(OpenDocumentReader, version) {
  EXPECT_TRUE(OpenDocumentReader::version().empty());
}

TEST(OpenDocumentReader, commit) {
  EXPECT_FALSE(OpenDocumentReader::commit().empty());
}

TEST(OpenDocumentReader, types_odt) {
  auto path = TestData::test_file_path("odr-public/odt/about.odt");
  auto types = OpenDocumentReader::types(path);
  EXPECT_EQ(types.size(), 2);
  EXPECT_EQ(types[0], FileType::zip);
  EXPECT_EQ(types[1], FileType::opendocument_text);
}

TEST(OpenDocumentReader, types_wpd) {
  auto path = TestData::test_file_path("odr-public/wpd/Sync3 Sample Page.wpd");
  auto types = OpenDocumentReader::types(path);
  EXPECT_EQ(types.size(), 1);
  EXPECT_EQ(types[0], FileType::word_perfect);
}
