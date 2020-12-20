#include <gtest/gtest.h>
#include <odr/exceptions.h>
#include <odr/file.h>

using namespace odr;

TEST(File, open) { EXPECT_THROW(File("/"), UnknownFileType); }

TEST(DocumentFile, open) { EXPECT_THROW(DocumentFile("/"), UnknownFileType); }
