#include <cfb/cfb_file.h>
#include <gtest/gtest.h>
#include <odr/exceptions.h>
#include <string>

using namespace odr::cfb;

TEST(CfbFile, open_fail) { EXPECT_THROW(CfbFile("/"), odr::NoCfbFile); }

TEST(CfbFile, open) {
  auto zip = std::make_shared<CfbFile>(
      "/home/andreas/workspace/OpenDocument.test/odt/style-various-1.odt");
}
