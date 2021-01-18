#include <gtest/gtest.h>
#include <odr/exceptions.h>
#include <string>
#include <zip/zip_file.h>

using namespace odr::zip;

TEST(ZipFile, open_directory) {
  EXPECT_ANY_THROW(ZipFile(std::make_shared<odr::common::DiscFile>("/")));
}

TEST(ZipFile, open_encrypted_docx) {
  EXPECT_THROW(
      ZipFile(std::make_shared<odr::common::DiscFile>(
          "/home/andreas/workspace/OpenDocument.test/docx/encrypted.docx")),
      odr::NoZipFile);
}

TEST(ZipFile, open_odt) {
  auto zip = std::make_shared<ZipFile>(std::make_shared<odr::common::DiscFile>(
      "/home/andreas/workspace/OpenDocument.test/odt/style-various-1.odt"));
}
