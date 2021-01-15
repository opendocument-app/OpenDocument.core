#include <common/path.h>
#include <gtest/gtest.h>
#include <odr/exceptions.h>
#include <string>
#include <zip/zip_file.h>

using namespace odr::zip;

TEST(miniz, list) {
  mz_zip_archive zip;

  memset(&zip, 0, sizeof(zip));
  const mz_bool status = mz_zip_reader_init_file(
      &zip, "/home/andreas/workspace/OpenDocument.test/odt/style-various-1.odt",
      0);
  EXPECT_TRUE(status);

  // TODO remove
  auto num_files = mz_zip_reader_get_num_files(&zip);
  for (int i = 0; i < num_files; ++i) {
    mz_zip_archive_file_stat stat{};
    mz_zip_reader_file_stat(&zip, i, &stat);
    std::cout << stat.m_is_directory << std::endl;
    std::cout << stat.m_filename << std::endl;
    std::cout << stat.m_method << std::endl;
    std::cout << std::endl;
  }
}

TEST(ZipFile, open_fail) { EXPECT_THROW(ZipFile("/"), odr::NoZipFile); }

TEST(ZipFile, open) {
  auto zip = std::make_shared<ZipFile>(
      "/home/andreas/workspace/OpenDocument.test/odt/style-various-1.odt");

  auto archive = zip->archive();
}
