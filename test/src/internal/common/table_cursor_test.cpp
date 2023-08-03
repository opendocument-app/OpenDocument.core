#include <gtest/gtest.h>

#include <internal/common/table_cursor.h>

#include <memory>

using namespace odr::internal::common;

TEST(TableCursor, test) {
  TableCursor cursor;
  EXPECT_EQ(0, cursor.row());
  EXPECT_EQ(0, cursor.column());
  cursor.add_row(1);
  EXPECT_EQ(1, cursor.row());
  EXPECT_EQ(0, cursor.column());
  cursor.add_cell(2, 2, 1);
  EXPECT_EQ(1, cursor.row());
  EXPECT_EQ(2, cursor.column());
  cursor.add_cell(2, 1, 1);
  EXPECT_EQ(1, cursor.row());
  EXPECT_EQ(4, cursor.column());
  cursor.add_row(1);
  EXPECT_EQ(2, cursor.row());
  EXPECT_EQ(2, cursor.column());
  cursor.add_cell(1, 1, 1);
  cursor.add_cell(1, 1, 1);
  EXPECT_EQ(2, cursor.row());
  EXPECT_EQ(4, cursor.column());
  cursor.add_row(1);
  EXPECT_EQ(3, cursor.row());
  EXPECT_EQ(0, cursor.column());
}
