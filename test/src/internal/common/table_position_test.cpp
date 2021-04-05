#include <gtest/gtest.h>
#include <internal/common/table_position.h>

using namespace odr::internal::common;

TEST(TablePosition, default) {
  TablePosition tp;
  EXPECT_EQ(0, tp.row());
  EXPECT_EQ(0, tp.column());
  EXPECT_EQ("A1", tp.to_string());
}

TEST(TablePosition, direct) {
  TablePosition tp(1, 2);
  EXPECT_EQ(1, tp.row());
  EXPECT_EQ(2, tp.column());
  EXPECT_EQ("C2", tp.to_string());
}

TEST(TablePosition, string1) {
  const std::string input = "A1";
  TablePosition tp(input);
  EXPECT_EQ(0, tp.row());
  EXPECT_EQ(0, tp.column());
  EXPECT_EQ(input, tp.to_string());
}

TEST(TablePosition, string2) {
  const std::string input = "AA11";
  TablePosition tp(input);
  EXPECT_EQ(10, tp.row());
  EXPECT_EQ(26, tp.column());
  EXPECT_EQ(input, tp.to_string());
}

TEST(TablePosition, string3) {
  const std::string input = "ZZ1";
  TablePosition tp(input);
  EXPECT_EQ(0, tp.row());
  EXPECT_EQ(701, tp.column());
  EXPECT_EQ(input, tp.to_string());
}

TEST(TablePosition, string4) {
  const std::string input = "AAA1";
  TablePosition tp(input);
  EXPECT_EQ(0, tp.row());
  EXPECT_EQ(702, tp.column());
  EXPECT_EQ(input, tp.to_string());
}
