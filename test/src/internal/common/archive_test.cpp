#include <odr/internal/cfb/cfb_archive.hpp>
#include <odr/internal/common/archive.hpp>
#include <odr/internal/zip/zip_archive.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

#include <memory>

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
