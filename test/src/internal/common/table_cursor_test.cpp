#include <gtest/gtest.h>
#include <internal/common/table_cursor.h>

using namespace odr::internal::common;

TEST(TableCursor, test) {
  TableCursor cursor;
  EXPECT_EQ(0, cursor.row());
  EXPECT_EQ(0, cursor.col());
  cursor.add_row(1);
  EXPECT_EQ(1, cursor.row());
  EXPECT_EQ(0, cursor.col());
  cursor.add_cell(2, 2, 1);
  EXPECT_EQ(1, cursor.row());
  EXPECT_EQ(2, cursor.col());
  cursor.add_cell(2, 1, 1);
  EXPECT_EQ(1, cursor.row());
  EXPECT_EQ(4, cursor.col());
  cursor.add_row(1);
  EXPECT_EQ(2, cursor.row());
  EXPECT_EQ(2, cursor.col());
  cursor.add_cell(1, 1, 1);
  cursor.add_cell(1, 1, 1);
  EXPECT_EQ(2, cursor.row());
  EXPECT_EQ(4, cursor.col());
  cursor.add_row(1);
  EXPECT_EQ(3, cursor.row());
  EXPECT_EQ(0, cursor.col());
}
