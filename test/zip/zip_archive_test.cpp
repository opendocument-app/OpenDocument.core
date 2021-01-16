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
}

TEST(ZipArchive, create_and_save) {
  auto archive = std::make_shared<ZipArchive>();

  std::ofstream out("test.zip");
  archive->save(out);
}
