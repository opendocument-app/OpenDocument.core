#include <gtest/gtest.h>
#include <odr/exceptions.h>
#include <odr/experimental/file.h>

using namespace odr;
using namespace odr::experimental;

TEST(File, open) { EXPECT_THROW(File("/"), FileNotFound); }

TEST(DocumentFile, open) { EXPECT_THROW(DocumentFile("/"), FileNotFound); }
