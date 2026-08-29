#include <odr/exceptions.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/zip/zip_archive.hpp>
#include <odr/internal/zip/zip_file.hpp>
#include <odr/internal/zip/zip_util.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <thread>
#include <vector>

using namespace odr;
using namespace odr::internal;
using namespace odr::internal::zip;
using namespace odr::test;

TEST(ZipArchive, open_directory) {
  EXPECT_ANY_THROW(ZipFile(std::make_shared<DiskFile>("/")));
}

TEST(ZipArchive, open_encrypted_docx) {
  EXPECT_THROW(ZipFile(std::make_shared<DiskFile>(
                   TestData::test_file_path("odr-public/docx/encrypted.docx"))),
               NoZipFile);
}

TEST(ZipArchive, open_odt) {
  ZipFile(std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/odt/style-various-1.odt")));
}

TEST(ZipArchive, open) {
  const util::Archive zip(std::make_shared<DiskFile>(
      TestData::test_file_path("odr-public/odt/style-various-1.odt")));

  EXPECT_EQ(19, std::vector(zip.begin(), zip.end()).size());
}

TEST(ZipArchive, create_and_save) {
  ZipArchive zip;

  zip.insert_file(std::end(zip), RelPath("a"),
                  std::make_shared<MemoryFile>("abc"));
  zip.insert_file(std::end(zip), RelPath("hi"),
                  std::make_shared<MemoryFile>("hello world!"));
  zip.insert_directory(std::end(zip), RelPath("b"));

  std::ofstream out("test.zip", std::ios::binary);
  zip.save(out);
}

TEST(ZipArchive, create) {
  const std::string path =
      (std::filesystem::current_path() / "created.zip").string();

  {
    ZipArchive zip;

    zip.insert_file(std::end(zip), RelPath("one.txt"),
                    std::make_shared<MemoryFile>("this is written at once"));
    zip.insert_file(
        std::end(zip), RelPath("two.txt"),
        std::make_shared<MemoryFile>("this is written at two stages"));

    zip.insert_directory(std::end(zip), RelPath("empty"));
    zip.insert_directory(std::end(zip), RelPath("notempty"));

    zip.insert_file(std::end(zip), RelPath("notempty/three.txt"),
                    std::make_shared<MemoryFile>("asdf"));
    zip.insert_file(std::end(zip), RelPath("./notempty/four.txt"),
                    std::make_shared<MemoryFile>("1234"));

    std::ofstream out(path, std::ios::binary);
    zip.save(out);
  }

  {
    auto zip =
        std::make_shared<util::Archive>(std::make_shared<DiskFile>(path));

    EXPECT_TRUE(zip->find(RelPath("one.txt"))->is_file());
    EXPECT_TRUE(zip->find(RelPath("two.txt"))->is_file());
    EXPECT_TRUE(zip->find(RelPath("empty"))->is_directory());
    EXPECT_TRUE(zip->find(RelPath("notempty"))->is_directory());
    EXPECT_TRUE(zip->find(RelPath("notempty/three.txt"))->is_file());
    EXPECT_TRUE(zip->find(RelPath("notempty/four.txt"))->is_file());
    EXPECT_TRUE(zip->find(RelPath("./notempty/four.txt"))->is_file());

    EXPECT_EQ(23, zip->find(RelPath("one.txt"))->file()->size());
    EXPECT_EQ(29, zip->find(RelPath("two.txt"))->file()->size());
    EXPECT_EQ(4, zip->find(RelPath("notempty/three.txt"))->file()->size());
    EXPECT_EQ(4, zip->find(RelPath("notempty/four.txt"))->file()->size());
    EXPECT_EQ(4, zip->find(RelPath("./notempty/four.txt"))->file()->size());
  }
}

TEST(ZipArchive, create_order) {
  const std::string path =
      (std::filesystem::current_path() / "created.zip").string();
  const std::vector<std::string> entries{"z", "one", "two", "three", "a", "0"};

  {
    ZipArchive zip;

    for (auto &&e : entries) {
      zip.insert_file(std::end(zip), RelPath(e),
                      std::make_shared<MemoryFile>(""));
    }

    std::ofstream out(path, std::ios::binary);
    zip.save(out);
  }

  {
    util::Archive zip(std::make_shared<DiskFile>(path));

    std::vector<std::string> actual;
    for (auto &&e : zip) {
      actual.push_back(e.path().string());
    }
    EXPECT_EQ(actual, entries);
  }
}

/// The read callback has to be re-entrant, for a memory and a stream source.
TEST(ZipArchive, concurrent_entry_reads) {
  const std::string path =
      (std::filesystem::current_path() / "concurrent.zip").string();
  constexpr std::size_t entry_count = 8;

  std::vector<std::string> expected;
  expected.reserve(entry_count);
  for (std::size_t i = 0; i < entry_count; ++i) {
    // large enough that a single entry spans many reads
    expected.emplace_back(64 * 1024, static_cast<char>('a' + i));
  }

  {
    ZipArchive zip;
    for (std::size_t i = 0; i < entry_count; ++i) {
      zip.insert_file(std::end(zip), RelPath("entry" + std::to_string(i)),
                      std::make_shared<MemoryFile>(expected[i]));
    }
    std::ofstream out(path, std::ios::binary);
    zip.save(out);
  }

  const std::vector<std::shared_ptr<abstract::File>> sources{
      std::make_shared<DiskFile>(path),
      std::make_shared<MemoryFile>(DiskFile(path))};

  for (const auto &source : sources) {
    const auto zip = std::make_shared<util::Archive>(source);

    std::vector<std::string> actual(entry_count);
    std::vector<std::thread> threads;
    threads.reserve(entry_count);
    for (std::size_t i = 0; i < entry_count; ++i) {
      threads.emplace_back([&zip, &actual, i] {
        const auto entry = zip->find(RelPath("entry" + std::to_string(i)));
        const auto stream = entry->file()->stream();
        actual[i] = std::string(std::istreambuf_iterator<char>(*stream),
                                std::istreambuf_iterator<char>());
      });
    }
    for (auto &thread : threads) {
      thread.join();
    }

    EXPECT_EQ(expected, actual);
  }
}

namespace {

std::uint32_t read_le(const std::string &data, const std::size_t offset,
                      const std::size_t size) {
  std::uint32_t value = 0;
  for (std::size_t i = 0; i < size; ++i) {
    value |=
        static_cast<std::uint32_t>(static_cast<std::uint8_t>(data[offset + i]))
        << (8 * i);
  }
  return value;
}

} // namespace

/// Walking the local headers by the sizes they carry only terminates if they
/// carry them.
TEST(ZipArchive, save_sizes_local_headers) {
  const std::string path =
      (std::filesystem::current_path() / "local_headers.zip").string();

  {
    ZipArchive zip;

    zip.insert_file(std::end(zip), RelPath("stored"),
                    std::make_shared<MemoryFile>("stored, like a mimetype"), 0);
    zip.insert_file(std::end(zip), RelPath("deflated"),
                    std::make_shared<MemoryFile>(std::string(1000, 'x')));
    zip.insert_directory(std::end(zip), RelPath("dir"));

    std::ofstream out(path, std::ios::binary);
    zip.save(out);
  }

  std::ifstream in(path, std::ios::binary);
  const std::string data{std::istreambuf_iterator<char>(in),
                         std::istreambuf_iterator<char>()};

  std::size_t offset = 0;
  std::size_t entries = 0;
  while (data.compare(offset, 4, "PK\x03\x04") == 0) {
    const std::uint32_t flag = read_le(data, offset + 6, 2);
    const std::uint32_t crc = read_le(data, offset + 14, 4);
    const std::uint32_t compressed_size = read_le(data, offset + 18, 4);
    const std::uint32_t path_size = read_le(data, offset + 26, 2);
    const std::uint32_t extra_size = read_le(data, offset + 28, 2);

    EXPECT_EQ(0, flag & 0x08);
    if (data.compare(offset + 30, path_size, "dir/") != 0) {
      EXPECT_NE(0, crc);
      EXPECT_NE(0, compressed_size);
    }

    offset += 30 + path_size + extra_size + compressed_size;
    ++entries;
  }

  EXPECT_EQ(3, entries);
}
