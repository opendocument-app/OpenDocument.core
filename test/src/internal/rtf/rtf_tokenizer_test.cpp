#include <gtest/gtest.h>

#include <odr/internal/rtf/rtf_token.hpp>
#include <odr/internal/rtf/rtf_tokenizer.hpp>

#include <array>
#include <sstream>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

using namespace odr::internal;

namespace {

/// One token as a short string, so a whole stream reads as a vector literal:
/// `{`/`}` for the groups, `\name` (with `(parameter)`) for a control word,
/// `\c` for a control symbol, `'hh` for a `\'hh` escape, `[bytes]` for text
/// and `#bytes` for a `\binN` payload.
std::string describe(const rtf::Token &token) {
  if (std::holds_alternative<rtf::GroupOpen>(token)) {
    return "{";
  }
  if (std::holds_alternative<rtf::GroupClose>(token)) {
    return "}";
  }
  if (const auto *word = std::get_if<rtf::ControlWord>(&token)) {
    std::string result = "\\" + word->name;
    if (word->parameter.has_value()) {
      result += "(" + std::to_string(*word->parameter) + ")";
    }
    return result;
  }
  if (const auto *symbol = std::get_if<rtf::ControlSymbol>(&token)) {
    return std::string("\\") + symbol->symbol;
  }
  if (const auto *hex = std::get_if<rtf::HexEscape>(&token)) {
    static constexpr std::array<char, 16> digits{'0', '1', '2', '3', '4', '5',
                                                 '6', '7', '8', '9', 'a', 'b',
                                                 'c', 'd', 'e', 'f'};
    const auto byte = static_cast<unsigned char>(hex->byte);
    return std::string("'") + digits.at(byte >> 4) + digits.at(byte & 0xf);
  }
  if (const auto *text = std::get_if<rtf::Text>(&token)) {
    return "[" + text->bytes + "]";
  }
  if (const auto *binary = std::get_if<rtf::Binary>(&token)) {
    return "#" + binary->bytes;
  }
  return "<end>";
}

std::vector<std::string> tokens(const std::string &content) {
  std::istringstream in(content);
  rtf::Tokenizer tokenizer(in);

  std::vector<std::string> result;
  while (true) {
    const rtf::Token token = tokenizer.read_token();
    if (std::holds_alternative<rtf::End>(token)) {
      return result;
    }
    result.push_back(describe(token));
  }
}

} // namespace

TEST(RtfTokenizer, groups_and_text) {
  EXPECT_EQ(tokens("{ab}"), (std::vector<std::string>{"{", "[ab]", "}"}));
}

TEST(RtfTokenizer, control_word_eats_its_delimiting_space) {
  EXPECT_EQ(tokens("\\par Hello"),
            (std::vector<std::string>{"\\par", "[Hello]"}));
}

TEST(RtfTokenizer, parameter_delimiter_follows_the_same_rule) {
  // the space after the parameter is consumed, so the text starts at `T`
  EXPECT_EQ(tokens("\\fs24 Text"),
            (std::vector<std::string>{"\\fs(24)", "[Text]"}));
  // anything else is left unread
  EXPECT_EQ(tokens("\\f0Hello"),
            (std::vector<std::string>{"\\f(0)", "[Hello]"}));
}

TEST(RtfTokenizer, a_parameter_is_part_of_the_control_word) {
  EXPECT_EQ(tokens("\\b0\\b "), (std::vector<std::string>{"\\b(0)", "\\b"}));
}

TEST(RtfTokenizer, a_control_symbol_takes_no_delimiter) {
  // the space after `\~` is text, not a delimiter
  EXPECT_EQ(tokens("\\~ x"), (std::vector<std::string>{"\\~", "[ x]"}));
  EXPECT_EQ(tokens("\\*\\foo"), (std::vector<std::string>{"\\*", "\\foo"}));
}

TEST(RtfTokenizer, escaped_braces_and_backslash) {
  EXPECT_EQ(tokens("\\{\\}\\\\"),
            (std::vector<std::string>{"\\{", "\\}", "\\\\"}));
}

TEST(RtfTokenizer, hex_escape) {
  EXPECT_EQ(tokens("\\'41\\'E4"), (std::vector<std::string>{"'41", "'e4"}));
  // `\'7b` is a byte, never a group open
  EXPECT_EQ(tokens("\\'7bx"), (std::vector<std::string>{"'7b", "[x]"}));
}

TEST(RtfTokenizer, an_invalid_hex_digit_throws) {
  EXPECT_THROW(tokens("\\'4z"), std::runtime_error);
}

TEST(RtfTokenizer, negative_parameter) {
  EXPECT_EQ(tokens("\\u-4064 ?"),
            (std::vector<std::string>{"\\u(-4064)", "[?]"}));
}

TEST(RtfTokenizer, a_bare_line_break_is_not_text) {
  EXPECT_EQ(tokens("a\r\nb"), (std::vector<std::string>{"[ab]"}));
}

TEST(RtfTokenizer, binary_payload_is_read_raw) {
  // the payload holds braces and a backslash, none of which are markup
  EXPECT_EQ(tokens("\\bin5 }{\\ab}"),
            (std::vector<std::string>{"#}{\\ab", "}"}));
}

TEST(RtfTokenizer, binary_without_a_parameter_is_empty) {
  EXPECT_EQ(tokens("\\bin x"), (std::vector<std::string>{"#", "[x]"}));
}

TEST(RtfTokenizer, a_binary_payload_running_past_the_end_throws) {
  EXPECT_THROW(tokens("\\bin9 ab"), std::runtime_error);
}

TEST(RtfTokenizer, a_trailing_backslash_throws) {
  EXPECT_THROW(tokens("ab\\"), std::runtime_error);
}
