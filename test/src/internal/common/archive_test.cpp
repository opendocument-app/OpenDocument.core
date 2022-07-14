#include <gtest/gtest.h>
#include <memory>
#include <odr/internal/cfb/cfb_archive.h>
#include <odr/internal/common/archive.h>
#include <odr/internal/common/file.h>
#include <odr/internal/zip/zip_archive.h>
#include <test_util.h>

using namespace odr::internal;
using namespace odr::internal::common;
using namespace odr::test;

TEST(Archive, smoke_zip) {
  Archive<zip::ReadonlyZipArchive>(
      zip::ReadonlyZipArchive(std::make_shared<DiskFile>(
          TestData::test_file_path("odr-public/odt/style-various-1.odt"))));
}

TEST(Archive, smoke_cfb) {
  Archive<cfb::ReadonlyCfbArchive>(
      cfb::ReadonlyCfbArchive(std::make_shared<MemoryFile>(DiskFile(
          TestData::test_file_path("odr-public/docx/encrypted.docx")))));
}
