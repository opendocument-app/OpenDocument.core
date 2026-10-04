#include <odr/internal/common/table_cursor.hpp>

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

using namespace odr::internal;

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

// A long-rowspan cell followed in a later row by rowspan cells at smaller
// columns used to leave the pending covered ranges unsorted, so the skip in
// `handle_rowspan_` missed them (infinite loop in the sheet renderer).
TEST(TableCursor, out_of_order_rowspans) {
  TableCursor cursor;
  cursor.add_cell(1, 1); // A1
  cursor.add_cell(1, 3); // B1, covers B2 + B3
  cursor.add_cell(1, 1); // C1
  cursor.add_row();
  EXPECT_EQ(0, cursor.column());
  cursor.add_cell(1, 2);         // A2, covers A3
  EXPECT_EQ(2, cursor.column()); // B2 covered, skipped
  cursor.add_cell(1, 1);         // C2
  cursor.add_row();
  EXPECT_EQ(2, cursor.column()); // A3 and B3 covered, skipped
}

TEST(TableCursor, long_spans_survive_skipped_rows) {
  constexpr auto max = std::numeric_limits<std::uint32_t>::max();
  TableCursor cursor;
  cursor.add_cell(2, max);
  cursor.add_row(max - 1);
  EXPECT_EQ(cursor.row(), max - 1);
  EXPECT_EQ(cursor.column(), 2);
  cursor.add_row();
  EXPECT_EQ(cursor.column(), 0);
}

TEST(TableCursor, rejects_invalid_extents_before_advancing) {
  constexpr auto max = std::numeric_limits<std::uint32_t>::max();
  TableCursor cursor;
  cursor.add_column(1);
  EXPECT_THROW(cursor.add_column(max), std::out_of_range);
  EXPECT_THROW(cursor.add_cell(max, 1, max), std::out_of_range);
  EXPECT_THROW(cursor.add_cell(0), std::out_of_range);
  EXPECT_THROW(cursor.add_cell(1, 0), std::out_of_range);
  EXPECT_EQ(cursor.column(), 1);
  EXPECT_EQ(cursor.row(), 0);
  cursor.add_row(max);
  EXPECT_THROW(cursor.add_row(), std::out_of_range);
  EXPECT_EQ(cursor.row(), max);
}
