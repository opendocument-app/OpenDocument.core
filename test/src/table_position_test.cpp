#include <odr/table_position.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace odr;

TEST(TablePosition, default) {
  const TablePosition tp;
  EXPECT_EQ(0, tp.row);
  EXPECT_EQ(0, tp.column);
  EXPECT_EQ("A1", tp.to_string());
}

TEST(TablePosition, direct) {
  const TablePosition tp(2, 1);
  EXPECT_EQ(2, tp.column);
  EXPECT_EQ(1, tp.row);
  EXPECT_EQ("C2", tp.to_string());
}

TEST(TablePosition, a_column_letter_is_read_without_case) {
  EXPECT_EQ(TablePosition::try_to_column_num("aa"), 26);
}

TEST(TablePosition, a_spelling_past_the_index_range_is_nothing) {
  EXPECT_FALSE(TablePosition::try_to_column_num("ABCDEFGHI").has_value());
  EXPECT_FALSE(TablePosition::try_to_row_num("99999999999").has_value());
  EXPECT_FALSE(TablePosition::try_to_row_num("0").has_value());
}

TEST(TablePosition, string1) {
  const std::string input = "A1";
  const TablePosition tp(input);
  EXPECT_EQ(0, tp.column);
  EXPECT_EQ(0, tp.row);
  EXPECT_EQ(input, tp.to_string());
}

TEST(TablePosition, string2) {
  const std::string input = "AA11";
  const TablePosition tp(input);
  EXPECT_EQ(26, tp.column);
  EXPECT_EQ(10, tp.row);
  EXPECT_EQ(input, tp.to_string());
}

TEST(TablePosition, string3) {
  const std::string input = "ZZ1";
  const TablePosition tp(input);
  EXPECT_EQ(701, tp.column);
  EXPECT_EQ(0, tp.row);
  EXPECT_EQ(input, tp.to_string());
}

TEST(TablePosition, string4) {
  const std::string input = "AAA1";
  const TablePosition tp(input);
  EXPECT_EQ(702, tp.column);
  EXPECT_EQ(0, tp.row);
  EXPECT_EQ(input, tp.to_string());
}
