#include <odr/internal/util/map_util.hpp>

#include <gtest/gtest.h>

#include <map>

using namespace odr::internal::util::map;

TEST(map_util, lookup_greater_than) {
  std::map<int, int> data = {{1, 1}, {2, 2}};
  EXPECT_EQ(data.find(1), lookup_greater_than(data, 0));
  EXPECT_EQ(data.find(2), lookup_greater_than(data, 1));
  EXPECT_EQ(std::end(data), lookup_greater_than(data, 2));
  EXPECT_EQ(std::end(data), lookup_greater_than(data, 3));
}

TEST(map_util, lookup_greater_or_equals) {
  std::map<int, int> data = {{1, 1}, {2, 2}};
  EXPECT_EQ(data.find(1), lookup_greater_or_equals(data, 0));
  EXPECT_EQ(data.find(1), lookup_greater_or_equals(data, 1));
  EXPECT_EQ(data.find(2), lookup_greater_or_equals(data, 2));
  EXPECT_EQ(std::end(data), lookup_greater_or_equals(data, 3));
}
