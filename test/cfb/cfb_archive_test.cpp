#include <cfb/cfb_archive.h>
#include <gtest/gtest.h>
#include <odr/exceptions.h>
#include <string>
#include <test/test_meta.h>

using namespace odr::cfb;
using namespace odr::test;

TEST(ReadonlyCfbArchive, open_directory) {
  EXPECT_ANY_THROW(ReadonlyCfbArchive(
      std::make_shared<odr::common::MemoryFile>(odr::common::DiscFile("/"))));
}

TEST(ReadonlyCfbArchive, open_odt) {
  EXPECT_THROW(ReadonlyCfbArchive(std::make_shared<odr::common::MemoryFile>(
                   odr::common::DiscFile(TestMeta::test_file_path(
                       "odr-public/odt/style-various-1.odt")))),
               odr::NoCfbFile);
}

TEST(ReadonlyCfbArchive, open_encrypted_docx) {
  ReadonlyCfbArchive(
      std::make_shared<odr::common::MemoryFile>(odr::common::DiscFile(
          TestMeta::test_file_path("odr-public/docx/encrypted.docx"))));
}
