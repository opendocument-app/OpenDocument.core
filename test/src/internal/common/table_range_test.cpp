#include <gtest/gtest.h>
#include <internal/common/table_range.h>

using namespace odr::internal::common;

TEST(TableRange, default) {
  TableRange tr;
  EXPECT_EQ(0, tr.from().row());
  EXPECT_EQ(0, tr.from().column());
  EXPECT_EQ(0, tr.to().row());
  EXPECT_EQ(0, tr.to().column());
  EXPECT_EQ("A1:A1", tr.to_string());
}

TEST(TableRange, string1) {
  const std::string input = "A1:C55";
  TableRange tr(input);
  EXPECT_EQ(0, tr.from().row());
  EXPECT_EQ(0, tr.from().column());
  EXPECT_EQ(54, tr.to().row());
  EXPECT_EQ(2, tr.to().column());
  EXPECT_EQ(input, tr.to_string());
}
