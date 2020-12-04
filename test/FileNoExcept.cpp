#include <gtest/gtest.h>
#include <odr/FileNoExcept.h>

using namespace odr;

TEST(FileNoExcept, open) {
  EXPECT_FALSE(FileNoExcept::open("/"));
}

TEST(DocumentFileNoExcept, open) {
    EXPECT_FALSE(DocumentFileNoExcept::open("/"));
}
