#include <gtest/gtest.h>
#include <test_util.hpp>

#include <odr/exceptions.hpp>

using namespace odr;
using namespace odr::test;

TEST(File, open) { EXPECT_THROW(File("/"), FileNotFound); }

TEST(DocumentFile, open) { EXPECT_THROW(DocumentFile("/"), FileNotFound); }

TEST(DecodedFile, wpd) {
  auto path = TestData::test_file_path("odr-public/wpd/Sync3 Sample Page.wpd");
  try {
    DecodedFile file(path);
    FAIL();
  } catch (const UnsupportedFileType &e) {
    EXPECT_EQ(e.file_type, FileType::word_perfect);
  }
}
