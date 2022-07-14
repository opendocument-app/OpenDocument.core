#include <gtest/gtest.h>
#include <memory>
#include <odr/internal/common/document_path.hpp>

using namespace odr::internal::common;

TEST(DocumentPath, empty) { EXPECT_EQ("", DocumentPath().to_string()); }

TEST(DocumentPath, example1) {
  EXPECT_EQ("/child:3/child:2/row:17/child:0",
            DocumentPath("/child:3/child:2/row:17/child:0").to_string());
}
