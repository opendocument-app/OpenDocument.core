#include <common/TableRange.h>
#include <gtest/gtest.h>

TEST(TableRange, default) {
  odr::common::TableRange tr("A1:C55");
  EXPECT_EQ(tr.from().row(), 0);
  EXPECT_EQ(tr.from().col(), 0);
  EXPECT_EQ(tr.to().row(), 54);
  EXPECT_EQ(tr.to().col(), 2);
}
