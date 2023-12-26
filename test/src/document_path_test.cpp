#include <odr/document_path.hpp>

#include <gtest/gtest.h>

using namespace odr;

TEST(DocumentPath, empty) { EXPECT_EQ("", DocumentPath().to_string()); }

TEST(DocumentPath, example1) {
  EXPECT_EQ("/child:3/child:2/row:17/child:0",
            DocumentPath("/child:3/child:2/row:17/child:0").to_string());
}
