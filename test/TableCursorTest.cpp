#include <common/TableCursor.h>
#include <gtest/gtest.h>

namespace odr {

TEST(TableCursor, test) {
  TableCursor tl;
  EXPECT_EQ(tl.getRow(), 0);
  EXPECT_EQ(tl.getCol(), 0);
  tl.addRow(1);
  EXPECT_EQ(tl.getRow(), 1);
  EXPECT_EQ(tl.getCol(), 0);
  tl.addCell(2, 2, 1);
  EXPECT_EQ(tl.getRow(), 1);
  EXPECT_EQ(tl.getCol(), 2);
  tl.addCell(2, 1, 1);
  EXPECT_EQ(tl.getRow(), 1);
  EXPECT_EQ(tl.getCol(), 4);
  tl.addRow(1);
  EXPECT_EQ(tl.getRow(), 2);
  EXPECT_EQ(tl.getCol(), 2);
  tl.addCell(1, 1, 1);
  tl.addCell(1, 1, 1);
  EXPECT_EQ(tl.getRow(), 2);
  EXPECT_EQ(tl.getCol(), 4);
  tl.addRow(1);
  EXPECT_EQ(tl.getRow(), 3);
  EXPECT_EQ(tl.getCol(), 0);
}

} // namespace odr
