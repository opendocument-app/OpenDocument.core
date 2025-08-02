#include <odr/exceptions.hpp>

#include <odr/internal/common/file.hpp>
#include <odr/internal/zip/zip_archive.hpp>
#include <odr/internal/zip/zip_file.hpp>
#include <odr/internal/zip/zip_util.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <memory>

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
  util::Archive zip(std::make_shared<DiskFile>(
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

  std::ofstream out("test.zip");
  zip.save(out);
}

TEST(ZipArchive, create) {
  const std::string path = std::filesystem::current_path() / "created.zip";

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

    std::ofstream out(path);
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
  const std::string path = std::filesystem::current_path() / "created.zip";
  const std::vector<std::string> entries{"z", "one", "two", "three", "a", "0"};

  {
    ZipArchive zip;

    for (auto &&e : entries) {
      zip.insert_file(std::end(zip), RelPath(e),
                      std::make_shared<MemoryFile>(""));
    }

    std::ofstream out(path);
    zip.save(out);
  }

  {
    util::Archive zip(std::make_shared<DiskFile>(path));

    auto it = entries.begin();
    for (auto &&e : zip) {
      EXPECT_EQ(Path(*it), e.path());
      ++it;
    }
  }
}
