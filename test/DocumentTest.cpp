#include <gtest/gtest.h>
#include <odr/Document.h>
#include <odr/Exception.h>

using namespace odr;

TEST(Document, open) { EXPECT_THROW(Document("/"), UnknownFileType); }

TEST(DocumentNoExcept, open) {
  EXPECT_EQ(nullptr, DocumentNoExcept::open("/"));
}
