#include <gtest/gtest.h>
#include <memory>
#include <odr/internal/util/xml_util.h>

using namespace odr::internal::util::xml;

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
  EXPECT_EQ(3, example5.size());
  EXPECT_EQ(StringToken::Type::tabs, example6[0].type);
  EXPECT_EQ("\t", example6[0].string);
  EXPECT_EQ(StringToken::Type::spaces, example6[1].type);
  EXPECT_EQ("  ", example6[1].string);
  EXPECT_EQ(StringToken::Type::tabs, example6[2].type);
  EXPECT_EQ("\t", example6[2].string);
}
