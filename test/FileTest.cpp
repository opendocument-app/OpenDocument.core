#include <gtest/gtest.h>
#include <odr/File.h>
#include <odr/Exception.h>

using namespace odr;

TEST(File, open) { EXPECT_THROW(File("/"), UnknownFileType); }

TEST(DocumentFile, open) { EXPECT_THROW(DocumentFile("/"), UnknownFileType); }
