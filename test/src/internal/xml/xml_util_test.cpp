#include <odr/internal/xml/xml_util.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>

using namespace odr::internal::xml;

TEST(xml_util, escape_text) {
  EXPECT_EQ("a &amp; b &lt;c&gt; \"d\"", escape_text("a & b <c> \"d\""));
}

TEST(xml_util, escape_attribute) {
  EXPECT_EQ("a&quot;b &amp; c", escape_attribute("a\"b & c"));
}

TEST(xml_util, escape_drops_control_characters) {
  EXPECT_EQ("ab\tc", escape_text(std::string("a\x01"
                                             "b\tc")));
}

TEST(xml_util, tokenize_text) {
  auto example1 = tokenize_text("hello world!");
  EXPECT_EQ(1, example1.size());
  EXPECT_EQ(StringToken::Type::string, example1[0].type);
  EXPECT_EQ("hello world!", example1[0].string);

  auto example2 = tokenize_text("hello  world!");
  EXPECT_EQ(3, example2.size());
  EXPECT_EQ(StringToken::Type::string, example2[0].type);
  EXPECT_EQ("hello", example2[0].string);
  EXPECT_EQ(StringToken::Type::spaces, example2[1].type);
  EXPECT_EQ("  ", example2[1].string);
  EXPECT_EQ(StringToken::Type::string, example2[2].type);
  EXPECT_EQ("world!", example2[2].string);

  auto example3 = tokenize_text("  ");
  EXPECT_EQ(1, example3.size());
  EXPECT_EQ(StringToken::Type::spaces, example3[0].type);
  EXPECT_EQ("  ", example3[0].string);

  auto example4 = tokenize_text("\t");
  EXPECT_EQ(1, example4.size());
  EXPECT_EQ(StringToken::Type::tabs, example4[0].type);
  EXPECT_EQ("\t", example4[0].string);

  auto example5 = tokenize_text("hello\tworld!");
  EXPECT_EQ(3, example5.size());
  EXPECT_EQ(StringToken::Type::string, example5[0].type);
  EXPECT_EQ("hello", example5[0].string);
  EXPECT_EQ(StringToken::Type::tabs, example5[1].type);
  EXPECT_EQ("\t", example5[1].string);
  EXPECT_EQ(StringToken::Type::string, example5[2].type);
  EXPECT_EQ("world!", example5[2].string);

  auto example6 = tokenize_text("\t  \t");
  EXPECT_EQ(3, example6.size());
  EXPECT_EQ(StringToken::Type::tabs, example6[0].type);
  EXPECT_EQ("\t", example6[0].string);
  EXPECT_EQ(StringToken::Type::spaces, example6[1].type);
  EXPECT_EQ("  ", example6[1].string);
  EXPECT_EQ(StringToken::Type::tabs, example6[2].type);
  EXPECT_EQ("\t", example6[2].string);
}

TEST(xml_util, tokenize_text_empty) { EXPECT_TRUE(tokenize_text("").empty()); }

// Only a run of two or more spaces is significant; a lone space is ordinary
// text, so it must not be split off as a `spaces` token.
TEST(xml_util, tokenize_text_single_space) {
  const auto tokens = tokenize_text("a b");
  ASSERT_EQ(1, tokens.size());
  EXPECT_EQ(StringToken::Type::string, tokens[0].type);
  EXPECT_EQ("a b", tokens[0].string);
}
