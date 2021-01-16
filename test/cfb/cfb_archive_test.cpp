#include <cfb/cfb_archive.h>
#include <cfb/cfb_file.h>
#include <fstream>
#include <gtest/gtest.h>
#include <string>

using namespace odr::zip;

TEST(CfbArchive, open) {
  auto cfb = std::make_shared<CfbFile>(
      "/home/andreas/workspace/OpenDocument.test/odt/style-various-1.odt");

  auto archive = cfb->archive();
}
