#include <cfb/cfb_archive.h>
#include <cfb/cfb_file.h>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

using namespace odr::cfb;

TEST(CfbArchive, open) {
  auto cfb = std::make_shared<CfbFile>(
      std::make_shared<odr::common::MemoryFile>(odr::common::DiscFile(
          "/home/andreas/workspace/OpenDocument.test/docx/encrypted.docx")));

  auto archive = cfb->archive();
}
