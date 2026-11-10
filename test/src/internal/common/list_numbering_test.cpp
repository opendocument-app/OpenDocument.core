#include <odr/internal/common/list_numbering.hpp>

#include <gtest/gtest.h>

#include <limits>

using namespace odr::internal;

namespace {

ListLevel decimal_level(std::string label, const std::uint32_t start = 1) {
  return {ListNumberFormat::decimal, std::move(label), start};
}

constexpr const char *symbol_dot = "\uF0B7";
constexpr const char *symbol_minus = "\uF02D";
constexpr const char *square = "\uF0A7";
constexpr const char *arrow = "\uF0D8";
constexpr const char *check = "\uF0FC";
constexpr const char *diamond = "\uF075";
constexpr const char *clover = "\uF076";
constexpr const char *white_square = "\uF0A8";
constexpr const char *unmapped = "\uF021";
constexpr const char *private_use = "\uE000";

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
  counter.restart(0, 0);
  EXPECT_EQ("0.", counter.advance(0, decimal_level("%1.", 0)));
  EXPECT_EQ("1.", counter.advance(0, decimal_level("%1.", 0)));
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
  const ListLevel level{ListNumberFormat::bullet, "%1•", 1};

  EXPECT_EQ("%1•", counter.advance(0, level));
  EXPECT_EQ("%1•", counter.advance(0, level));
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

TEST(ListCounter, rejects_overflow) {
  ListCounter counter;
  constexpr auto maximum = std::numeric_limits<std::uint32_t>::max();
  EXPECT_EQ("4294967295.", counter.advance(0, decimal_level("%1.", maximum)));
  EXPECT_THROW(counter.advance(0, decimal_level("%1.")), std::overflow_error);
  EXPECT_EQ(maximum, counter.number(0));
  EXPECT_THROW(counter.advance(maximum, decimal_level("%1.")),
               std::overflow_error);
  EXPECT_THROW(counter.restart(maximum, 0), std::overflow_error);
}

TEST(ListCounter, expands_the_level_placeholder) {
  ListCounter counter;
  const ListLevel level = decimal_level(
      std::string{level_placeholder, '\x01', '.', level_placeholder, '\x0a'});

  for (std::uint32_t skipped = 0; skipped < 9; ++skipped) {
    counter.start(skipped, level);
  }
  EXPECT_EQ("1.1", counter.advance(9, level));
}

TEST(ListCounter, leaves_a_braced_level_in_lvl_text_alone) {
  ListCounter counter;
  EXPECT_EQ("%{1}1", counter.advance(0, decimal_level("%{1}%1")));
}

TEST(ListCounter, starts_a_level_without_counting) {
  ListCounter counter;
  const ListLevel outer = decimal_level("%1.", 3);
  const ListLevel inner = decimal_level("%1.%2");

  counter.start(0, outer);
  EXPECT_EQ("3.1", counter.advance(1, inner));
  EXPECT_EQ("4.", counter.advance(0, outer));
  EXPECT_TRUE(counter.is_open(0));
  counter.start(0, outer);
  EXPECT_EQ("5.", counter.advance(0, outer));
}

TEST(ResolveBullet, a_symbol_font_bullet_becomes_its_unicode_character) {
  EXPECT_EQ("•", resolve_bullet(symbol_dot, 0, "Symbol"));
  EXPECT_EQ("•", resolve_bullet(symbol_dot, 1, "'Symbol'"));
  EXPECT_EQ("−", resolve_bullet(symbol_minus, 0, "symbol"));
  EXPECT_EQ("▪", resolve_bullet(square, 1, "Wingdings"));
  EXPECT_EQ("➢", resolve_bullet(arrow, 0, "Wingdings"));
  EXPECT_EQ("✓", resolve_bullet(check, 0, "Wingdings"));
  EXPECT_EQ("◆", resolve_bullet(diamond, 0, "Wingdings"));
  EXPECT_EQ("❖", resolve_bullet(clover, 0, "Wingdings"));
  EXPECT_EQ("◻", resolve_bullet(white_square, 0, "Wingdings"));
}

TEST(ResolveBullet, the_font_tells_the_same_byte_apart) {
  EXPECT_EQ("♣", resolve_bullet(square, 0, "Symbol"));
  EXPECT_EQ("▪", resolve_bullet(square, 0, "Wingdings"));
}

TEST(ResolveBullet, a_symbol_font_is_read_at_its_byte_too) {
  EXPECT_EQ("❖", resolve_bullet("v", 0, "Wingdings"));
  EXPECT_EQ("▪", resolve_bullet("§", 0, "Wingdings"));
  EXPECT_EQ("■", resolve_bullet("n", 0, "ZapfDingbats"));
  EXPECT_EQ("−", resolve_bullet("-", 0, "Symbol"));
}

TEST(ResolveBullet, an_unmapped_symbol_takes_the_level_default) {
  EXPECT_EQ("◦", resolve_bullet(unmapped, 1, "Wingdings"));
  EXPECT_EQ("▪", resolve_bullet("x", 2, "Wingdings"));
  // Wingdings 2 has a table of its own
  EXPECT_EQ("•", resolve_bullet(square, 0, "Wingdings 2"));
}

TEST(ResolveBullet, a_private_use_bullet_in_another_font_takes_the_default) {
  EXPECT_EQ("•", resolve_bullet(private_use, 0, "OpenSymbol"));
  EXPECT_EQ("◦", resolve_bullet(square, 1, ""));
  EXPECT_EQ("▪", resolve_bullet(square, 2, "Marlett"));
}

TEST(ResolveBullet, other_text_is_kept) {
  EXPECT_EQ("–", resolve_bullet("–", 0, "Symbol"));
  EXPECT_EQ("o", resolve_bullet("o", 1, "Courier New"));
  EXPECT_EQ("v", resolve_bullet("v", 1, ""));
  EXPECT_EQ("◦", resolve_bullet("", 1));
  EXPECT_EQ("•", resolve_bullet("\xff", 0));
}
