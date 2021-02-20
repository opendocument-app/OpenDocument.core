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
