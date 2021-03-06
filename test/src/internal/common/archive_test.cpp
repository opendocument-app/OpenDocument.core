#include <gtest/gtest.h>
#include <internal/cfb/cfb_archive.h>
#include <internal/common/archive.h>
#include <internal/zip/zip_archive.h>
#include <test_util.h>

using namespace odr::internal;
using namespace odr::internal::common;
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
