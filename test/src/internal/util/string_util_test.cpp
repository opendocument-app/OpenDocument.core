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

  // Empty input is one empty segment, never zero segments.
  {
    const std::vector<std::string> strings = split("", ",");
    EXPECT_EQ(strings.size(), 1);
    EXPECT_EQ(strings[0], "");
  }
}

TEST(string_util, trim_view) {
  EXPECT_EQ(trim_view("  abc  "), "abc");
  EXPECT_EQ(ltrim_view("  abc  "), "abc  ");
  EXPECT_EQ(rtrim_view("  abc  "), "  abc");

  // No surrounding whitespace leaves the content untouched.
  EXPECT_EQ(trim_view("abc"), "abc");

  // Interior whitespace is preserved.
  EXPECT_EQ(trim_view("\t a b c \n"), "a b c");

  // Empty and all-whitespace inputs collapse to an empty view.
  EXPECT_EQ(trim_view(""), "");
  EXPECT_EQ(trim_view(" \t\r\n "), "");

  // The result is a subrange of the input, so the leading offset is the
  // distance between the data pointers.
  {
    const std::string_view input = "   abc ";
    const std::string_view trimmed = trim_view(input);
    EXPECT_EQ(trimmed, "abc");
    EXPECT_EQ(trimmed.data() - input.data(), 3);
  }
}

TEST(string_util, trim_view_custom_predicate) {
  const auto is_dot = [](const char c) { return c == '.'; };

  EXPECT_EQ(trim_view("..abc..", is_dot), "abc");
  // The default ASCII-space predicate does not treat '.' as whitespace.
  EXPECT_EQ(trim_view("..abc.."), "..abc..");
  // Conversely, the dot predicate does not strip spaces.
  EXPECT_EQ(trim_view("  abc  ", is_dot), "  abc  ");
}

TEST(string_util, trim_owning_delegates_to_view) {
  EXPECT_EQ(trim("  abc  "), "abc");
  EXPECT_EQ(ltrim("  abc  "), "abc  ");
  EXPECT_EQ(rtrim("  abc  "), "  abc");

  const auto is_dot = [](const char c) { return c == '.'; };
  EXPECT_EQ(trim("..abc..", is_dot), "abc");

  std::string s = "  abc  ";
  trim_inplace(s);
  EXPECT_EQ(s, "abc");

  std::string d = "..abc..";
  trim_inplace(d, is_dot);
  EXPECT_EQ(d, "abc");
}

TEST(string_util, to_lower) {
  EXPECT_EQ(to_lower('A'), 'a');
  EXPECT_EQ(to_lower('a'), 'a');
  EXPECT_EQ(to_lower('7'), '7');

  EXPECT_EQ(to_lower("MiXeD Case 42"), "mixed case 42");
  EXPECT_EQ(to_lower(""), "");

  // Bytes above ascii keep their value - a utf-8 sequence must survive
  // unchanged rather than be folded byte by byte.
  EXPECT_EQ(to_lower("ÄÖÜ"), "ÄÖÜ");
}

TEST(string_util, equals_ignore_case) {
  EXPECT_TRUE(equals_ignore_case("script", "SCRIPT"));
  EXPECT_TRUE(equals_ignore_case("", ""));
  EXPECT_FALSE(equals_ignore_case("script", "scrip"));
  EXPECT_FALSE(equals_ignore_case("script", "scripts"));
  EXPECT_FALSE(equals_ignore_case("script", "style"));
}

TEST(string_util, starts_with_ignore_case) {
  EXPECT_TRUE(starts_with_ignore_case("DATA:image/png", "data:"));
  EXPECT_TRUE(starts_with_ignore_case("abc", ""));
  EXPECT_TRUE(starts_with_ignore_case("abc", "abc"));
  // A prefix longer than the string matches nothing, empty string included.
  EXPECT_FALSE(starts_with_ignore_case("ab", "abc"));
  EXPECT_FALSE(starts_with_ignore_case("", "a"));
  EXPECT_FALSE(starts_with_ignore_case("xabc", "abc"));
}

TEST(string_util, find_ignore_case) {
  EXPECT_EQ(find_ignore_case("a @IMPORT b", "@import"), 2);
  EXPECT_EQ(find_ignore_case("abc", "d"), std::string_view::npos);

  // The search starts at `from`, and the result is an index into the whole
  // string.
  EXPECT_EQ(find_ignore_case("url(url(", "URL(", 1), 4);
  EXPECT_EQ(find_ignore_case("url(url(", "URL(", 5), std::string_view::npos);

  // `from` past the end is not an out-of-range read.
  EXPECT_EQ(find_ignore_case("abc", "a", 99), std::string_view::npos);
}
