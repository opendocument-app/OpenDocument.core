#include <common/table_range.h>
#include <gtest/gtest.h>

TEST(TableRange, default) {
  odr::common::TableRange tr;
  EXPECT_EQ(0, tr.from().row());
  EXPECT_EQ(0, tr.from().col());
  EXPECT_EQ(0, tr.to().row());
  EXPECT_EQ(0, tr.to().col());
  EXPECT_EQ("A1:A1", tr.to_string());
}

TEST(TableRange, string1) {
  const std::string input = "A1:C55";
  odr::common::TableRange tr(input);
  EXPECT_EQ(0, tr.from().row());
  EXPECT_EQ(0, tr.from().col());
  EXPECT_EQ(54, tr.to().row());
  EXPECT_EQ(2, tr.to().col());
  EXPECT_EQ(input, tr.to_string());
}
