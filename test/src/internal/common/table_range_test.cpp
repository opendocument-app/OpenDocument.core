#include <odr/internal/common/table_range.hpp>

#include <gtest/gtest.h>

#include <string>

using namespace odr::internal;

TEST(TableRange, default) {
  const TableRange tr;
  EXPECT_EQ(0, tr.from().column);
  EXPECT_EQ(0, tr.from().row);
  EXPECT_EQ(0, tr.to().column);
  EXPECT_EQ(0, tr.to().row);
  EXPECT_EQ("A1:A1", tr.to_string());
}

TEST(TableRange, string1) {
  const std::string input = "A1:C55";
  const TableRange tr(input);
  EXPECT_EQ(0, tr.from().column);
  EXPECT_EQ(0, tr.from().row);
  EXPECT_EQ(2, tr.to().column);
  EXPECT_EQ(54, tr.to().row);
  EXPECT_EQ(input, tr.to_string());
}

/// `to` is the last position of the range, not one past it.
TEST(TableRange, contains) {
  const TableRange tr("B2:D4");
  EXPECT_TRUE(tr.contains({1, 1}));
  EXPECT_TRUE(tr.contains({3, 3}));
  EXPECT_TRUE(tr.contains({2, 3}));
  EXPECT_FALSE(tr.contains({0, 1}));
  EXPECT_FALSE(tr.contains({1, 0}));
  EXPECT_FALSE(tr.contains({4, 3}));
  EXPECT_FALSE(tr.contains({3, 4}));
}
