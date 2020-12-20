#include <gtest/gtest.h>
#include <odr/file_no_except_test.h>

using namespace odr;

TEST(FileNoExcept, open) { EXPECT_FALSE(FileNoExcept::open("/")); }

TEST(DocumentFileNoExcept, open) {
  EXPECT_FALSE(DocumentFileNoExcept::open("/"));
}
