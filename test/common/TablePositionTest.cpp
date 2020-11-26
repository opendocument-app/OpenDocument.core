#include <common/TablePosition.h>
#include <gtest/gtest.h>

TEST(TablePosition, default) {
  odr::common::TablePosition tp;
  EXPECT_EQ(tp.row(), 0);
  EXPECT_EQ(tp.col(), 0);
}

TEST(TablePosition, direct) {
  odr::common::TablePosition tp(1, 2);
  EXPECT_EQ(tp.row(), 1);
  EXPECT_EQ(tp.col(), 2);
}

TEST(TablePosition, string1) {
  odr::common::TablePosition tp("A1");
  EXPECT_EQ(tp.row(), 0);
  EXPECT_EQ(tp.col(), 0);
}

TEST(TablePosition, string2) {
  const std::string input = "AA11";
  odr::common::TablePosition tp(input);
  EXPECT_EQ(tp.row(), 10);
  EXPECT_EQ(tp.col(), 26);
  EXPECT_EQ(tp.toString(), input);
}

TEST(TablePosition, string3) {
  const std::string input = "ZZ1";
  odr::common::TablePosition tp(input);
  EXPECT_EQ(tp.row(), 0);
  EXPECT_EQ(tp.col(), 701);
  EXPECT_EQ(tp.toString(), input);
}

TEST(TablePosition, string4) {
  const std::string input = "AAA1";
  odr::common::TablePosition tp(input);
  EXPECT_EQ(tp.row(), 0);
  EXPECT_EQ(tp.col(), 702);
  EXPECT_EQ(tp.toString(), input);
}
