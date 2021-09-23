#include <gtest/gtest.h>
#include <internal/common/document_path.h>

using namespace odr::internal::common;

TEST(DocumentPath, empty) { EXPECT_EQ("", DocumentPath().to_string()); }
