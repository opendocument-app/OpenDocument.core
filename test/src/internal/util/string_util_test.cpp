#include <odr/internal/util/string_util.hpp>

#include <gtest/gtest.h>

using namespace odr::internal::util::string;

TEST(string_util, split) {
  const std::string string = "abc";

  {
    const std::vector<std::string> strings = split(string, "b");
    EXPECT_EQ(strings.size(), 2);
    EXPECT_EQ(strings[0], "a");
    EXPECT_EQ(strings[1], "c");
  }

  {
    const std::vector<std::string> strings = split(string, "a");
    EXPECT_EQ(strings.size(), 2);
    EXPECT_EQ(strings[0], "");
    EXPECT_EQ(strings[1], "bc");
  }

  {
    const std::vector<std::string> strings = split(string, "c");
    EXPECT_EQ(strings.size(), 2);
    EXPECT_EQ(strings[0], "ab");
    EXPECT_EQ(strings[1], "");
  }

  {
    const std::vector<std::string> strings = split(string, "d");
    EXPECT_EQ(strings.size(), 1);
    EXPECT_EQ(strings[0], "abc");
  }

  {
    const std::vector<std::string> strings = split(string, "abc");
    EXPECT_EQ(strings.size(), 2);
    EXPECT_EQ(strings[0], "");
    EXPECT_EQ(strings[1], "");
  }
}
