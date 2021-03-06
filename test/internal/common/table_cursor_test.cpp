#include <gtest/gtest.h>
#include <internal/common/table_cursor.h>

using namespace odr::internal::common;

TEST(TableCursor, test) {
  TableCursor tl;
  EXPECT_EQ(0, tl.row());
  EXPECT_EQ(0, tl.col());
  tl.add_row(1);
  EXPECT_EQ(1, tl.row());
  EXPECT_EQ(0, tl.col());
  tl.add_cell(2, 2, 1);
  EXPECT_EQ(1, tl.row());
  EXPECT_EQ(2, tl.col());
  tl.add_cell(2, 1, 1);
  EXPECT_EQ(1, tl.row());
  EXPECT_EQ(4, tl.col());
  tl.add_row(1);
  EXPECT_EQ(2, tl.row());
  EXPECT_EQ(2, tl.col());
  tl.add_cell(1, 1, 1);
  tl.add_cell(1, 1, 1);
  EXPECT_EQ(2, tl.row());
  EXPECT_EQ(4, tl.col());
  tl.add_row(1);
  EXPECT_EQ(3, tl.row());
  EXPECT_EQ(0, tl.col());
}
