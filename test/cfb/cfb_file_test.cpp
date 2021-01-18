#include <cfb/cfb_file.h>
#include <gtest/gtest.h>
#include <odr/exceptions.h>
#include <string>

using namespace odr::cfb;

TEST(CfbFile, open_directory) {
  EXPECT_ANY_THROW(CfbFile(
      std::make_shared<odr::common::MemoryFile>(odr::common::DiscFile("/"))));
}

TEST(CfbFile, open_odt) {
  EXPECT_THROW(
      CfbFile(std::make_shared<odr::common::MemoryFile>(
          odr::common::DiscFile("/home/andreas/workspace/OpenDocument.test/odt/"
                                "style-various-1.odt"))),
      odr::NoCfbFile);
}

TEST(CfbFile, open_encrypted_docx) {
  auto cfb = std::make_shared<CfbFile>(
      std::make_shared<odr::common::MemoryFile>(odr::common::DiscFile(
          "/home/andreas/workspace/OpenDocument.test/docx/encrypted.docx")));
}
