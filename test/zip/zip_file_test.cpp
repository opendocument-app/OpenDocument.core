#include <gtest/gtest.h>
#include <odr/exceptions.h>
#include <string>
#include <zip/zip_file.h>

using namespace odr::zip;

TEST(ZipFile, open_fail) { EXPECT_THROW(ZipFile("/"), odr::NoZipFile); }

TEST(ZipFile, open) {
  auto zip = std::make_shared<ZipFile>(
      "/home/andreas/workspace/OpenDocument.test/odt/style-various-1.odt");
}
