#include <odr/exceptions.hpp>

#include <odr/internal/cfb/cfb_archive.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <memory>

using namespace odr::internal;
using namespace odr::test;

TEST(ReadonlyCfbArchive, open_directory) {
  EXPECT_ANY_THROW(cfb::ReadonlyCfbArchive(
      std::make_shared<common::MemoryFile>(common::DiskFile("/"))));
}

TEST(ReadonlyCfbArchive, open_odt) {
  EXPECT_THROW(cfb::ReadonlyCfbArchive(std::make_shared<common::MemoryFile>(
                   common::DiskFile(TestData::test_file_path(
                       "odr-public/odt/style-various-1.odt")))),
               odr::NoCfbFile);
}

TEST(ReadonlyCfbArchive, open_encrypted_docx) {
  cfb::ReadonlyCfbArchive cfb(
      std::make_shared<common::MemoryFile>(common::DiskFile(
          TestData::test_file_path("odr-public/docx/encrypted.docx"))));

  EXPECT_TRUE(cfb.find("EncryptionInfo") == std::end(cfb));
  EXPECT_TRUE(cfb.find("/EncryptionInfo") != std::end(cfb));
}
