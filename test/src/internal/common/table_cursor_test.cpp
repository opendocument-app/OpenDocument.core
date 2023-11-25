#include <gtest/gtest.h>

#include <odr/internal/common/table_cursor.hpp>

#include <memory>

using namespace odr::internal::common;

TEST(TableCursor, test) {
  TableCursor cursor;
  EXPECT_EQ(0, cursor.column());
  EXPECT_EQ(0, cursor.row());
  cursor.add_row(1);
  EXPECT_EQ(0, cursor.column());
  EXPECT_EQ(1, cursor.row());
  cursor.add_cell(2, 2, 1);
  EXPECT_EQ(2, cursor.column());
  EXPECT_EQ(1, cursor.row());
  cursor.add_cell(2, 1, 1);
  EXPECT_EQ(4, cursor.column());
  EXPECT_EQ(1, cursor.row());
  cursor.add_row(1);
  EXPECT_EQ(2, cursor.column());
  EXPECT_EQ(2, cursor.row());
  cursor.add_cell(1, 1, 1);
  cursor.add_cell(1, 1, 1);
  EXPECT_EQ(4, cursor.column());
  EXPECT_EQ(2, cursor.row());
  cursor.add_row(1);
  EXPECT_EQ(0, cursor.column());
  EXPECT_EQ(3, cursor.row());
}
