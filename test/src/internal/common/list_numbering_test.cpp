#include <odr/internal/common/list_numbering.hpp>

#include <gtest/gtest.h>

using namespace odr::internal;

namespace {

ListLevel decimal_level(std::string label, const std::uint32_t start = 1) {
  return {ListNumberFormat::decimal, std::move(label), start};
}

} // namespace

TEST(FormatListNumber, decimal) {
  EXPECT_EQ("1", format_list_number(ListNumberFormat::decimal, 1));
  EXPECT_EQ("42", format_list_number(ListNumberFormat::decimal, 42));
  EXPECT_EQ("09", format_list_number(ListNumberFormat::decimal_zero, 9));
  EXPECT_EQ("10", format_list_number(ListNumberFormat::decimal_zero, 10));
}

TEST(FormatListNumber, letter) {
  EXPECT_EQ("a", format_list_number(ListNumberFormat::letter_lower, 1));
  EXPECT_EQ("z", format_list_number(ListNumberFormat::letter_lower, 26));
  EXPECT_EQ("aa", format_list_number(ListNumberFormat::letter_lower, 27));
  EXPECT_EQ("ab", format_list_number(ListNumberFormat::letter_lower, 28));
  EXPECT_EQ("AA", format_list_number(ListNumberFormat::letter_upper, 27));
}

TEST(FormatListNumber, roman) {
  EXPECT_EQ("i", format_list_number(ListNumberFormat::roman_lower, 1));
  EXPECT_EQ("iv", format_list_number(ListNumberFormat::roman_lower, 4));
  EXPECT_EQ("XIV", format_list_number(ListNumberFormat::roman_upper, 14));
  EXPECT_EQ("MCMXCIV", format_list_number(ListNumberFormat::roman_upper, 1994));
  EXPECT_EQ("4000", format_list_number(ListNumberFormat::roman_upper, 4000));
}

TEST(FormatListNumber, without_a_number) {
  EXPECT_EQ("", format_list_number(ListNumberFormat::none, 1));
  EXPECT_EQ("", format_list_number(ListNumberFormat::bullet, 1));
}

TEST(ListCounter, counts_up_within_a_level) {
  ListCounter counter;
  const ListLevel level = decimal_level("%1.");

  EXPECT_EQ("1.", counter.advance(0, level));
  EXPECT_EQ("2.", counter.advance(0, level));
  EXPECT_EQ("3.", counter.advance(0, level));
}

TEST(ListCounter, starts_at_the_level_start_value) {
  ListCounter counter;
  const ListLevel level = decimal_level("%1.", 5);

  EXPECT_EQ("5.", counter.advance(0, level));
  EXPECT_EQ("6.", counter.advance(0, level));
}

TEST(ListCounter, resets_deeper_levels) {
  ListCounter counter;
  const ListLevel outer = decimal_level("%1.");
  const ListLevel inner = decimal_level("%1.%2.");

  EXPECT_EQ("1.", counter.advance(0, outer));
  EXPECT_EQ("1.1.", counter.advance(1, inner));
  EXPECT_EQ("1.2.", counter.advance(1, inner));
  EXPECT_EQ("2.", counter.advance(0, outer));
  EXPECT_EQ("2.1.", counter.advance(1, inner));
}

TEST(ListCounter, expands_each_level_in_its_own_format) {
  ListCounter counter;
  const ListLevel outer{ListNumberFormat::roman_upper, "%1.", 1};
  const ListLevel inner{ListNumberFormat::letter_lower, "%1.%2)", 1};

  EXPECT_EQ("I.", counter.advance(0, outer));
  EXPECT_EQ("I.a)", counter.advance(1, inner));
  EXPECT_EQ("I.b)", counter.advance(1, inner));
}

TEST(ListCounter, treats_a_bullet_label_as_literal) {
  ListCounter counter;
  const ListLevel level{ListNumberFormat::bullet, "•", 1};

  EXPECT_EQ("•", counter.advance(0, level));
  EXPECT_EQ("•", counter.advance(0, level));
}

TEST(ListCounter, restarts_where_told_to) {
  ListCounter counter;
  const ListLevel level = decimal_level("%1.");

  EXPECT_EQ("1.", counter.advance(0, level));
  EXPECT_EQ("2.", counter.advance(0, level));
  counter.restart(0, 0);
  EXPECT_EQ("1.", counter.advance(0, level));
  counter.restart(0, 9);
  EXPECT_EQ("10.", counter.advance(0, level));
}

TEST(ListCounter, keeps_a_stray_percent_literal) {
  ListCounter counter;
  const ListLevel level = decimal_level("%1 of 100%");

  EXPECT_EQ("1 of 100%", counter.advance(0, level));
}

TEST(ListCounter, starts_deep_without_its_ancestors) {
  ListCounter counter;
  const ListLevel level = decimal_level("%1.%2.%3.");

  EXPECT_EQ("1.1.1.", counter.advance(2, level));
}
