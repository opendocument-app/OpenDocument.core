#include <odr/odr.hpp>

#include <test_util.hpp>

#include <gtest/gtest.h>

using namespace odr;
using namespace odr::internal;
using namespace odr::test;

TEST(odr, version) { EXPECT_TRUE(odr::version().empty()); }

TEST(odr, commit) { EXPECT_FALSE(odr::commit().empty()); }

TEST(odr, types_odt) {
  auto path = TestData::test_file_path("odr-public/odt/about.odt");
  auto types = odr::types(path);
  EXPECT_EQ(types.size(), 2);
  EXPECT_EQ(types[0], FileType::zip);
  EXPECT_EQ(types[1], FileType::opendocument_text);
}

TEST(odr, types_wpd) {
  auto path = TestData::test_file_path("odr-public/wpd/Sync3 Sample Page.wpd");
  auto types = odr::types(path);
  EXPECT_EQ(types.size(), 1);
  EXPECT_EQ(types[0], FileType::word_perfect);
}
