#include <odr/exceptions.hpp>
#include <odr/file.hpp>

#include <odr/internal/common/file.hpp>

#include <test_util.hpp>

#include <memory>
#include <string>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::test;

TEST(File, open) { EXPECT_THROW(File("/"), FileNotFound); }

/// `MemoryFile` used to report itself as `disk`, and `memory_data()` handed
/// back a bare pointer that only meant something alongside `size()`.
TEST(File, memory_file_reports_memory_and_its_bytes) {
  const File file(std::make_shared<internal::MemoryFile>(std::string("hello")));

  EXPECT_EQ(file.location(), FileLocation::memory);
  EXPECT_EQ(file.size(), 5);
  EXPECT_FALSE(file.disk_path().has_value());
  ASSERT_TRUE(file.memory_data().has_value());
  EXPECT_EQ(*file.memory_data(), "hello");
}

TEST(File, disk_file_has_no_memory_data) {
  const File file(std::make_shared<internal::DiskFile>(
      TestData::test_file_path("odr-public/odt/about.odt")));

  EXPECT_EQ(file.location(), FileLocation::disk);
  EXPECT_TRUE(file.disk_path().has_value());
  EXPECT_FALSE(file.memory_data().has_value());
}

TEST(DocumentFile, open) { EXPECT_THROW(DocumentFile("/"), FileNotFound); }

TEST(DecodedFile, wpd) {
  const auto logger = Logger::create_stdio("odr-test", LogLevel::verbose);

  const auto path =
      TestData::test_file_path("odr-public/wpd/Sync3 Sample Page.wpd");
  try {
    DecodedFile file(path, logger);
    FAIL();
  } catch (const UnsupportedFileType &e) {
    EXPECT_EQ(e.file_type, FileType::word_perfect);
  }
}
