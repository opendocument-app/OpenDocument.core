#include <odr/exceptions.hpp>

#include <odr/internal/cfb/cfb_archive.hpp>
#include <odr/internal/cfb/cfb_file.hpp>
#include <odr/internal/cfb/cfb_util.hpp>
#include <odr/internal/common/file.hpp>
#include <odr/internal/util/stream_util.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <memory>
#include <string_view>
#include <vector>

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

TEST(CfbArchive, an_entry_stream_outlives_its_file_wrapper) {
  auto archive =
      std::make_shared<cfb::util::Archive>(std::make_shared<DiskFile>(
          TestData::test_file_path("odr-public/docx/encrypted.docx")));
  const std::string expected =
      MemoryFile(*archive->find(RelPath("EncryptionInfo"))->file()).content();
  const auto stream =
      archive->find(RelPath("EncryptionInfo"))->file()->stream();
  archive.reset();
  EXPECT_EQ(internal::util::stream::read(*stream), expected);
}

TEST(CfbArchive, nested_children_finish_before_outer_siblings) {
  impl::CompoundFileHeader header{};
  std::memcpy(header.signature.data(), impl::CompoundFileReader::MAGIC, 8);
  header.minor_version = 0x3e;
  header.major_version = 3;
  header.sector_shift = 9;
  header.byte_order = 0xfffe;
  header.mini_sector_shift = 6;
  header.mini_stream_cutoff_size = 4096;
  header.num_fat_sector = 1;
  header.first_mini_fat_sector_location = 0xfffffffe;
  header.first_difat_sector_location = 0xfffffffe;
  header.first_directory_sector_location = 1;
  header.header_difat.fill(impl::NullId);
  header.header_difat[0] = 0;

  std::array<std::uint32_t, 128> fat{};
  fat.fill(impl::NullId);
  fat[0] = 0xfffffffd;
  fat[1] = 2;
  fat[2] = 0xfffffffe;

  std::array<impl::CompoundFileEntry, 6> entries{};
  const std::array<std::u16string_view, 6> names{u"Root Entry", u"AA", u"CC",
                                                 u"D",          u"BB", u"DD"};
  for (std::size_t i = 0; i < entries.size(); ++i) {
    auto &entry = entries[i];
    entry.left_sibling_id = entry.right_sibling_id = entry.child_id =
        impl::NullId;
    entry.start_sector_location = 0xfffffffe;
    std::ranges::copy(names[i], entry.name.begin());
    entry.name_len = static_cast<std::uint16_t>((names[i].size() + 1) * 2);
    entry.type = 2;
    entry.color_flag = 1;
  }
  entries[0].type = 5;
  entries[0].child_id = 2;
  entries[1].type = 1;
  entries[1].child_id = 3;
  entries[1].right_sibling_id = 4;
  entries[2].left_sibling_id = 1;
  entries[2].right_sibling_id = 5;
  entries[4].color_flag = 0;

  std::string bytes(2048, '\0');
  std::memcpy(bytes.data(), &header, sizeof(header));
  std::memcpy(bytes.data() + 512, fat.data(), sizeof(fat));
  std::memcpy(bytes.data() + 1024, entries.data(), sizeof(entries));
  const cfb::util::Archive archive(
      std::make_shared<MemoryFile>(std::move(bytes)));
  std::vector<std::string> paths;
  for (const auto &entry : archive) {
    paths.push_back(entry.path().string());
  }
  EXPECT_EQ(paths,
            (std::vector<std::string>{"", "AA", "AA/D", "BB", "CC", "DD"}));
}
