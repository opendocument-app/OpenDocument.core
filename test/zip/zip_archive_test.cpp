#include <common/file.h>
#include <fstream>
#include <gtest/gtest.h>
#include <odr/exceptions.h>
#include <string>
#include <test/test_util.h>
#include <zip/zip_archive.h>

using namespace odr;
using namespace odr::zip;
using namespace odr::common;
using namespace odr::test;

TEST(ReadonlyZipArchive, open_directory) {
  EXPECT_ANY_THROW(ReadonlyZipArchive(std::make_shared<DiscFile>("/")));
}

TEST(ReadonlyZipArchive, open_encrypted_docx) {
  EXPECT_THROW(ReadonlyZipArchive(std::make_shared<DiscFile>(
                   TestData::test_file_path("odr-public/docx/encrypted.docx"))),
               NoZipFile);
}

TEST(ReadonlyZipArchive, open_odt) {
  ReadonlyZipArchive(std::make_shared<DiscFile>(
      TestData::test_file_path("odr-public/odt/style-various-1.odt")));
}

TEST(ReadonlyZipArchive, open) {
  ReadonlyZipArchive zip(std::make_shared<DiscFile>(
      TestData::test_file_path("odr-public/odt/style-various-1.odt")));

  for (auto &&e : zip) {
    std::cout << e.path() << std::endl;
  }
}

TEST(ZipArchive, create_and_save) {
  ZipArchive zip;

  zip.insert_file(std::end(zip), "a", std::make_shared<MemoryFile>("abc"));
  zip.insert_file(std::end(zip), "hi",
                  std::make_shared<MemoryFile>("hello world!"));
  zip.insert_directory(std::end(zip), "b");

  std::ofstream out("test.zip");
  zip.save(out);
}

TEST(ZipArchive, create) {
  const std::string path = "created.zip";

  {
    ZipArchive zip;

    zip.insert_file(std::end(zip), "one.txt",
                    std::make_shared<MemoryFile>("this is written at once"));
    zip.insert_file(
        std::end(zip), "two.txt",
        std::make_shared<MemoryFile>("this is written at two stages"));

    zip.insert_directory(std::end(zip), "empty");
    zip.insert_directory(std::end(zip), "notempty");

    zip.insert_file(std::end(zip), "notempty/three.txt",
                    std::make_shared<MemoryFile>("asdf"));
    zip.insert_file(std::end(zip), "./notempty/four.txt",
                    std::make_shared<MemoryFile>("1234"));

    std::ofstream out(path);
    zip.save(out);
  }

  {
    ReadonlyZipArchive zip(std::make_shared<DiscFile>(path));

    EXPECT_TRUE(zip.find("one.txt")->is_file());
    EXPECT_TRUE(zip.find("two.txt")->is_file());
    EXPECT_TRUE(zip.find("empty")->is_directory());
    EXPECT_TRUE(zip.find("notempty")->is_directory());
    EXPECT_TRUE(zip.find("notempty/three.txt")->is_file());
    EXPECT_TRUE(zip.find("notempty/four.txt")->is_file());
    EXPECT_TRUE(zip.find("./notempty/four.txt")->is_file());

    EXPECT_EQ(23, zip.find("one.txt")->file()->size());
    EXPECT_EQ(29, zip.find("two.txt")->file()->size());
    EXPECT_EQ(4, zip.find("notempty/three.txt")->file()->size());
    EXPECT_EQ(4, zip.find("notempty/four.txt")->file()->size());
    EXPECT_EQ(4, zip.find("./notempty/four.txt")->file()->size());
  }
}

TEST(ZipArchive, create_order) {
  const std::string path = "created.zip";
  const std::vector<std::string> entries{"z", "one", "two", "three", "a", "0"};

  {
    ZipArchive zip;

    for (auto &&e : entries) {
      zip.insert_file(std::end(zip), e, std::make_shared<MemoryFile>(""));
    }

    std::ofstream out(path);
    zip.save(out);
  }

  {
    ReadonlyZipArchive zip(std::make_shared<DiscFile>(path));

    auto it = entries.begin();
    for (auto &&e : zip) {
      EXPECT_EQ(*it, e.path().string());
      ++it;
    }
  }
}
