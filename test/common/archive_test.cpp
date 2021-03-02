#include <cfb/cfb_archive.h>
#include <common/archive.h>
#include <gtest/gtest.h>
#include <test/test_util.h>
#include <zip/zip_archive.h>

using namespace odr;
using namespace odr::common;
using namespace odr::test;

TEST(Archive, smoke_zip) {
  Archive<zip::ReadonlyZipArchive>(
      zip::ReadonlyZipArchive(std::make_shared<DiscFile>(
          TestData::test_file_path("odr-public/odt/style-various-1.odt"))));
}

TEST(Archive, smoke_cfb) {
  Archive<cfb::ReadonlyCfbArchive>(
      cfb::ReadonlyCfbArchive(std::make_shared<MemoryFile>(DiscFile(
          TestData::test_file_path("odr-public/docx/encrypted.docx")))));
}
