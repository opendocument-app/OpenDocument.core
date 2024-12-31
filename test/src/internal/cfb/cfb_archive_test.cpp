#include <odr/exceptions.hpp>

#include <odr/internal/cfb/cfb_archive.hpp>
#include <odr/internal/cfb/cfb_file.hpp>
#include <odr/internal/cfb/cfb_util.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <memory>

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::common;
using namespace odr::internal::cfb;
using namespace odr::test;

TEST(CfbArchive, open_directory) {
  EXPECT_ANY_THROW(
      CfbFile(std::make_shared<common::MemoryFile>(common::DiskFile("/"))));
}

TEST(CfbArchive, open_odt) {
  EXPECT_THROW(
      CfbFile(std::make_shared<common::MemoryFile>(common::DiskFile(
          TestData::test_file_path("odr-public/odt/style-various-1.odt")))),
      odr::NoCfbFile);
}

TEST(CfbArchive, open_encrypted_docx) {
  util::Archive cfb(std::make_shared<common::MemoryFile>(common::DiskFile(
      TestData::test_file_path("odr-public/docx/encrypted.docx"))));

  EXPECT_TRUE(cfb.find(Path("EncryptionInfo")) == std::end(cfb));
  EXPECT_TRUE(cfb.find(Path("/EncryptionInfo")) != std::end(cfb));
}
