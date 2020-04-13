#include <common/TableCursor.h>
#include <gtest/gtest.h>

TEST(TableCursor, test) {
  odr::common::TableCursor tl;
  EXPECT_EQ(tl.row(), 0);
  EXPECT_EQ(tl.col(), 0);
  tl.addRow(1);
  EXPECT_EQ(tl.row(), 1);
  EXPECT_EQ(tl.col(), 0);
  tl.addCell(2, 2, 1);
  EXPECT_EQ(tl.row(), 1);
  EXPECT_EQ(tl.col(), 2);
  tl.addCell(2, 1, 1);
  EXPECT_EQ(tl.row(), 1);
  EXPECT_EQ(tl.col(), 4);
  tl.addRow(1);
  EXPECT_EQ(tl.row(), 2);
  EXPECT_EQ(tl.col(), 2);
  tl.addCell(1, 1, 1);
  tl.addCell(1, 1, 1);
  EXPECT_EQ(tl.row(), 2);
  EXPECT_EQ(tl.col(), 4);
  tl.addRow(1);
  EXPECT_EQ(tl.row(), 3);
  EXPECT_EQ(tl.col(), 0);
}
