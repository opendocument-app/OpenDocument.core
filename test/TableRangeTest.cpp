#include <common/TableRange.h>
#include <gtest/gtest.h>

TEST(TableRange, default) {
  odr::common::TableRange tr("A1:C55");
  EXPECT_EQ(tr.from().getRow(), 0);
  EXPECT_EQ(tr.from().getCol(), 0);
  EXPECT_EQ(tr.to().getRow(), 54);
  EXPECT_EQ(tr.to().getCol(), 2);
}
