#include <common/file.h>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <zip/zip_archive.h>
#include <zip/zip_file.h>

using namespace odr::zip;

TEST(ZipArchive, open) {
  auto zip = std::make_shared<ZipFile>(
      "/home/andreas/workspace/OpenDocument.test/odt/style-various-1.odt");

  auto archive = zip->archive();

  for (auto it = archive->begin(); !it->equals(*archive->end()); it->next()) {
    std::cout << it->entry()->path() << std::endl;
  }
}

TEST(ZipArchive, create_and_save) {
  auto archive = std::make_shared<ZipArchive>();

  archive->insert_file(archive->end(), "a",
                       std::make_shared<odr::common::MemoryFile>("abc"));
  archive->insert_file(
      archive->end(), "hi",
      std::make_shared<odr::common::MemoryFile>("hello world!"));
  archive->insert_directory(archive->end(), "b");

  std::ofstream out("test.zip");
  archive->save(out);
}
