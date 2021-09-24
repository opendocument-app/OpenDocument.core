#include <gtest/gtest.h>
#include <internal/cfb/cfb_archive.h>
#include <internal/common/file.h>
#include <iterator>
#include <memory>
#include <odr/exceptions.h>
#include <test_util.h>

using namespace odr::internal;
using namespace odr::test;

TEST(ReadonlyCfbArchive, open_directory) {
  EXPECT_ANY_THROW(cfb::ReadonlyCfbArchive(
      std::make_shared<common::MemoryFile>(common::DiscFile("/"))));
}

TEST(ReadonlyCfbArchive, open_odt) {
  EXPECT_THROW(cfb::ReadonlyCfbArchive(std::make_shared<common::MemoryFile>(
                   common::DiscFile(TestData::test_file_path(
                       "odr-public/odt/style-various-1.odt")))),
               odr::NoCfbFile);
}

TEST(ReadonlyCfbArchive, open_encrypted_docx) {
  cfb::ReadonlyCfbArchive cfb(
      std::make_shared<common::MemoryFile>(common::DiscFile(
          TestData::test_file_path("odr-public/docx/encrypted.docx"))));

  EXPECT_TRUE(cfb.find("EncryptionInfo") == std::end(cfb));
  EXPECT_TRUE(cfb.find("/EncryptionInfo") != std::end(cfb));
}
