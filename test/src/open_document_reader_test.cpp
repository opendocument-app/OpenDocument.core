#include <gtest/gtest.h>
#include <odr/open_document_reader.h>

using namespace odr;

TEST(OpenDocumentReader, version) {
  EXPECT_FALSE(OpenDocumentReader::version().empty());
}

TEST(OpenDocumentReader, commit) {
  EXPECT_FALSE(OpenDocumentReader::commit().empty());
}
