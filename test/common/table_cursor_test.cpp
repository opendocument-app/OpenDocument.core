#include <common/table_cursor.h>
#include <gtest/gtest.h>

TEST(TableCursor, test) {
  odr::common::TableCursor tl;
  EXPECT_EQ(tl.row(), 0);
  EXPECT_EQ(tl.col(), 0);
  tl.add_row(1);
  EXPECT_EQ(tl.row(), 1);
  EXPECT_EQ(tl.col(), 0);
  tl.add_cell(2, 2, 1);
  EXPECT_EQ(tl.row(), 1);
  EXPECT_EQ(tl.col(), 2);
  tl.add_cell(2, 1, 1);
  EXPECT_EQ(tl.row(), 1);
  EXPECT_EQ(tl.col(), 4);
  tl.add_row(1);
  EXPECT_EQ(tl.row(), 2);
  EXPECT_EQ(tl.col(), 2);
  tl.add_cell(1, 1, 1);
  tl.add_cell(1, 1, 1);
  EXPECT_EQ(tl.row(), 2);
  EXPECT_EQ(tl.col(), 4);
  tl.add_row(1);
  EXPECT_EQ(tl.row(), 3);
  EXPECT_EQ(tl.col(), 0);
}
