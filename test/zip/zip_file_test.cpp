#include <common/path.h>
#include <gtest/gtest.h>
#include <string>
#include <zip/zip_file.h>

using namespace odr::zip;

TEST(ZipFile, exception) {
  auto zip = std::make_shared<ZipFile>(
      "/home/andreas/workspace/OpenDocument.test/odt/style-various-1.odt");
}
