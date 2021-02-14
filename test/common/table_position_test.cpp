#include <common/table_position.h>
#include <gtest/gtest.h>

TEST(TablePosition, default) {
  odr::common::TablePosition tp;
  EXPECT_EQ(0, tp.row());
  EXPECT_EQ(0, tp.col());
  EXPECT_EQ("A1", tp.to_string());
}

TEST(TablePosition, direct) {
  odr::common::TablePosition tp(1, 2);
  EXPECT_EQ(1, tp.row());
  EXPECT_EQ(2, tp.col());
  EXPECT_EQ("A2", tp.to_string());
}

TEST(TablePosition, string1) {
  const std::string input = "A1";
  odr::common::TablePosition tp(input);
  EXPECT_EQ(0, tp.row());
  EXPECT_EQ(0, tp.col());
  EXPECT_EQ(input, tp.to_string());
}

TEST(TablePosition, string2) {
  const std::string input = "AA11";
  odr::common::TablePosition tp(input);
  EXPECT_EQ(10, tp.row());
  EXPECT_EQ(26, tp.col());
  EXPECT_EQ(input, tp.to_string());
}

TEST(TablePosition, string3) {
  const std::string input = "ZZ1";
  odr::common::TablePosition tp(input);
  EXPECT_EQ(0, tp.row());
  EXPECT_EQ(701, tp.col());
  EXPECT_EQ(input, tp.to_string());
}

TEST(TablePosition, string4) {
  const std::string input = "AAA1";
  odr::common::TablePosition tp(input);
  EXPECT_EQ(0, tp.row());
  EXPECT_EQ(702, tp.col());
  EXPECT_EQ(input, tp.to_string());
}
