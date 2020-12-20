#include <gtest/gtest.h>
#include <odr/Exceptions.h>
#include <odr/File.h>

using namespace odr;

TEST(File, open) { EXPECT_THROW(File("/"), UnknownFileType); }

TEST(DocumentFile, open) { EXPECT_THROW(DocumentFile("/"), UnknownFileType); }
