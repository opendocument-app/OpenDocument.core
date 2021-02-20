#include <common/file.h>
#include <fstream>
#include <gtest/gtest.h>
#include <odr/exceptions.h>
#include <string>
#include <zip/zip_archive.h>

using namespace odr::zip;

TEST(ReadonlyZipArchive, open_directory) {
  EXPECT_ANY_THROW(
      ReadonlyZipArchive(std::make_shared<odr::common::DiscFile>("/")));
}

TEST(ReadonlyZipArchive, open_encrypted_docx) {
  EXPECT_THROW(
      ReadonlyZipArchive(std::make_shared<odr::common::DiscFile>(
          "/home/andreas/workspace/OpenDocument.test/docx/encrypted.docx")),
      odr::NoZipFile);
}

TEST(ReadonlyZipArchive, open_odt) {
  ReadonlyZipArchive(std::make_shared<odr::common::DiscFile>(
      "/home/andreas/workspace/OpenDocument.test/odt/style-various-1.odt"));
}

TEST(ReadonlyZipArchive, open) {
  ReadonlyZipArchive zip(std::make_shared<odr::common::DiscFile>(
      "/home/andreas/workspace/OpenDocument.test/odt/style-various-1.odt"));

  for (auto &&e : zip) {
    std::cout << e.path() << std::endl;
  }
}

TEST(ZipArchive, create_and_save) {
  ZipArchive zip;

  zip.insert_file(std::end(zip), "a",
                  std::make_shared<odr::common::MemoryFile>("abc"));
  zip.insert_file(std::end(zip), "hi",
                  std::make_shared<odr::common::MemoryFile>("hello world!"));
  zip.insert_directory(std::end(zip), "b");

  std::ofstream out("test.zip");
  zip.save(out);
}

TEST(ZipArchive, create) {
  const std::string path = "created.zip";

  {
    ZipArchive zip;

    {
      const auto sink = zip.write("one.txt");
      sink->write("this is written at once", 23);
    }

    {
      const auto sink = zip.write("two.txt");
      sink->write("this is written ", 15);
      sink->write("at two stages", 13);
    }

    zip.createDirectory("empty");
    zip.createDirectory("notempty");

    {
      const auto sink = zip.write("notempty/three.txt");
      sink->write("asdf", 4);
    }

    {
      const auto sink = zip.write("./notempty/four.txt");
      sink->write("1234", 4);
    }

    zip.save(path);
  }

  {
    ReadonlyZipArchive zip(path);

    EXPECT_TRUE(reader.isFile("one.txt"));
    EXPECT_TRUE(reader.isFile("two.txt"));
    EXPECT_TRUE(reader.isDirectory("empty"));
    EXPECT_TRUE(reader.isDirectory("notempty"));
    EXPECT_TRUE(reader.isFile("notempty/three.txt"));
    EXPECT_TRUE(reader.isFile("notempty/four.txt"));
    EXPECT_TRUE(reader.isFile("./notempty/four.txt"));

    EXPECT_EQ(23, reader.size("one.txt"));
    EXPECT_EQ(28, reader.size("two.txt"));
    EXPECT_EQ(4, reader.size("notempty/three.txt"));
    EXPECT_EQ(4, reader.size("notempty/four.txt"));
    EXPECT_EQ(4, reader.size("./notempty/four.txt"));
  }
}

TEST(ZipArchive, create_order) {
  const std::string file = "created.zip";
  const std::vector<std::string> entries{"z", "one", "two", "three", "a", "0"};

  {
    ZipArchive writer(file);

    for (auto &&e : entries) {
      writer.write(e);
    }
  }

  {
    ReadonlyZipArchive reader(file);

    auto it = entries.begin();
    reader.visit([&](const auto &path) {
      EXPECT_EQ(*it, path.string());
      ++it;
    });
  }
}
