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

TEST(map_util, lookup_on_empty_map) {
  const std::map<int, int> data;
  EXPECT_EQ(std::end(data), lookup_greater_than(data, 0));
  EXPECT_EQ(std::end(data), lookup_greater_or_equals(data, 0));
}

// A miss leaves the out parameter untouched.
TEST(map_util, lookup_reports_hit_and_miss) {
  const std::map<int, int> data = {{1, 10}};

  int value = -1;
  EXPECT_TRUE(lookup(data, 1, value));
  EXPECT_EQ(10, value);
  EXPECT_FALSE(lookup(data, 2, value));
  EXPECT_EQ(10, value);
}

TEST(map_util, lookup_default_falls_back) {
  const std::map<int, int> data = {{1, 10}};

  int value = -1;
  EXPECT_TRUE(lookup_default(data, 1, value, 99));
  EXPECT_EQ(10, value);
  EXPECT_FALSE(lookup_default(data, 2, value, 99));
  EXPECT_EQ(99, value);

  EXPECT_EQ(10, lookup_default(data, 1, 99));
  EXPECT_EQ(99, lookup_default(data, 2, 99));
}
