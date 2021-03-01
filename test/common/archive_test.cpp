#include <cfb/cfb_archive.h>
#include <common/archive.h>
#include <gtest/gtest.h>
#include <zip/zip_archive.h>

using namespace odr;
using namespace odr::common;

TEST(Archive, smoke_zip) {
  Archive<zip::ReadonlyZipArchive>(zip::ReadonlyZipArchive(std::make_shared<
                                                           DiscFile>(
      "/home/andreas/workspace/OpenDocument.test/odt/style-various-1.odt")));
}

TEST(Archive, smoke_cfb) {
  Archive<cfb::ReadonlyCfbArchive>(
      cfb::ReadonlyCfbArchive(std::make_shared<MemoryFile>(DiscFile(
          "/home/andreas/workspace/OpenDocument.test/docx/encrypted.docx"))));
}
