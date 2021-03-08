#include <gtest/gtest.h>
#include <odr/experimental/file_no_except.h>

using namespace odr;
using namespace odr::experimental;

TEST(FileNoExcept, open) { EXPECT_FALSE(FileNoExcept::open("/")); }

TEST(DocumentFileNoExcept, open) {
  EXPECT_FALSE(DocumentFileNoExcept::open("/"));
}
