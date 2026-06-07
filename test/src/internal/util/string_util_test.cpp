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

  // Multiple delimiters: exercises the middle-segment path (last_end > 0).
  {
    const std::vector<std::string> strings = split("a,bb,ccc,d", ",");
    EXPECT_EQ(strings.size(), 4);
    EXPECT_EQ(strings[0], "a");
    EXPECT_EQ(strings[1], "bb");
    EXPECT_EQ(strings[2], "ccc");
    EXPECT_EQ(strings[3], "d");
  }

  // Multi-character delimiter with multiple occurrences.
  {
    const std::vector<std::string> strings = split("one::two::three", "::");
    EXPECT_EQ(strings.size(), 3);
    EXPECT_EQ(strings[0], "one");
    EXPECT_EQ(strings[1], "two");
    EXPECT_EQ(strings[2], "three");
  }

  // Consecutive delimiters produce empty segments in between.
  {
    const std::vector<std::string> strings = split("x\r\ry", "\r");
    EXPECT_EQ(strings.size(), 3);
    EXPECT_EQ(strings[0], "x");
    EXPECT_EQ(strings[1], "");
    EXPECT_EQ(strings[2], "y");
  }
}
