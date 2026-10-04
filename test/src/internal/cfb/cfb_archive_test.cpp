#include <odr/exceptions.hpp>

#include <odr/internal/cfb/cfb_archive.hpp>
#include <odr/internal/cfb/cfb_file.hpp>
#include <odr/internal/cfb/cfb_util.hpp>
#include <odr/internal/common/file.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <memory>

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::cfb;
using namespace odr::test;

TEST(CfbArchive, open_directory) {
  EXPECT_ANY_THROW(CfbFile(std::make_shared<DiskFile>("/")));
}

TEST(CfbArchive, open_odt) {
  EXPECT_THROW(CfbFile(std::make_shared<DiskFile>(TestData::test_file_path(
                   "odr-public/odt/style-various-1.odt"))),
               odr::NoCfbFile);
}

TEST(CfbArchive, open_encrypted_docx) {
  cfb::util::Archive cfb(std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/docx/encrypted.docx")));

  EXPECT_TRUE(cfb.find(RelPath("Encryption")) == std::end(cfb));
  EXPECT_TRUE(cfb.find(RelPath("EncryptionInfo")) != std::end(cfb));
}

TEST(CfbArchive, seeks_use_the_consumed_position_in_small_and_large_streams) {
  const auto archive =
      std::make_shared<cfb::util::Archive>(std::make_shared<DiskFile>(
          TestData::test_file_path("odr-public/docx/encrypted.docx")));
  for (const char *name : {"EncryptionInfo", "EncryptedPackage"}) {
    const auto file = archive->find(RelPath(name))->file();
    const auto data = MemoryFile(*file).content();
    ASSERT_GT(data.size(), 8);
    const auto in = file->stream();
    EXPECT_EQ(in->tellg(), 0);
    EXPECT_EQ(in->get(), static_cast<unsigned char>(data[0]));
    EXPECT_EQ(in->tellg(), 1);
    in->seekg(3, std::ios::cur);
    EXPECT_EQ(in->get(), static_cast<unsigned char>(data[4]));
    in->seekg(-3, std::ios::cur);
    EXPECT_EQ(in->get(), static_cast<unsigned char>(data[2]));
    in->seekg(-1, std::ios::end);
    EXPECT_EQ(in->get(), static_cast<unsigned char>(data.back()));
    EXPECT_EQ(in->tellg(), static_cast<std::streamoff>(data.size()));
    in->seekg(std::numeric_limits<std::streamoff>::max(), std::ios::cur);
    EXPECT_TRUE(in->fail());
    in->clear();
    in->seekg(std::numeric_limits<std::streamoff>::min(), std::ios::end);
    EXPECT_TRUE(in->fail());
  }
}
