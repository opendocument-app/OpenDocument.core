#include <gtest/gtest.h>
#include <odr/open_document_reader.h>

using namespace odr;

TEST(OpenDocumentReader, version) {
  // TODO check if not empty
  EXPECT_EQ("", OpenDocumentReader::version());
}

TEST(OpenDocumentReader, commit) {
  // TODO check if not empty
  EXPECT_EQ("", OpenDocumentReader::commit());
}
