#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <sstream>
#include <string>
#include <string_view>
#include <utility>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

namespace {
std::string read_hex_string(const std::string &input) {
  std::istringstream in(input);
  ObjectParser parser(in);
  return std::get<HexString>(parser.read_string()).string;
}

Real read_number(const std::string &input) {
  std::istringstream in(input);
  ObjectParser parser(in);
  return parser.read_number();
}

UnsignedInteger read_unsigned_integer(const std::string &input) {
  std::istringstream in(input);
  ObjectParser parser(in);
  return parser.read_unsigned_integer();
}

bool peek_unsigned_integer(const std::string &input) {
  std::istringstream in(input);
  ObjectParser parser(in);
  return parser.peek_unsigned_integer();
}

Integer read_integer(const std::string &input) {
  std::istringstream in(input);
  ObjectParser parser(in);
  return parser.read_integer();
}

// Runs skip_past(marker) on `input` and reports whether the marker was found
// together with the bytes left after the cursor, so a test can pin both the
// result and the resulting position.
std::pair<bool, std::string> skip_past(const std::string &input,
                                       const std::string_view marker) {
  std::istringstream in(input);
  ObjectParser parser(in);
  const bool found = parser.skip_past(marker);
  std::string rest;
  while (parser.geti() != ObjectParser::eof) {
    rest.push_back(parser.bumpc());
  }
  return {found, rest};
}
} // namespace

// 7.3.3: a real is an optional integer part, a `.`, and an optional fractional
// part; either part may be absent (but not both).
TEST(PdfObjectParser, read_number) {
  EXPECT_DOUBLE_EQ(read_number("3.14"), 3.14);
  EXPECT_DOUBLE_EQ(read_number("0.5"), 0.5);
  EXPECT_DOUBLE_EQ(read_number("42."), 42.0);
  EXPECT_DOUBLE_EQ(read_number(".25"), 0.25);
  EXPECT_DOUBLE_EQ(read_number("10"), 10.0);
  // sign applies to the whole magnitude, and either part may be absent
  EXPECT_DOUBLE_EQ(read_number("-1.5"), -1.5);
  EXPECT_DOUBLE_EQ(read_number("-.5"), -0.5);
  EXPECT_DOUBLE_EQ(read_number("+.5"), 0.5);
  EXPECT_DOUBLE_EQ(read_number("-7"), -7.0);
}

// 7.3.3: an unsigned integer is one or more digits. A missing number is an
// error rather than a silent 0.
TEST(PdfObjectParser, read_unsigned_integer) {
  EXPECT_EQ(read_unsigned_integer("123"), 123u);
  EXPECT_EQ(read_unsigned_integer("0"), 0u);
  EXPECT_EQ(read_unsigned_integer("007"), 7u);
  EXPECT_EQ(read_unsigned_integer("42 0 obj"), 42u);
  EXPECT_ANY_THROW(read_unsigned_integer("abc"));
  EXPECT_ANY_THROW(read_unsigned_integer("-5"));
  EXPECT_ANY_THROW(read_unsigned_integer(""));
}

TEST(PdfObjectParser, peek_unsigned_integer) {
  EXPECT_TRUE(peek_unsigned_integer("5"));
  EXPECT_TRUE(peek_unsigned_integer("0xyz"));
  EXPECT_FALSE(peek_unsigned_integer("-5"));
  EXPECT_FALSE(peek_unsigned_integer("+5"));
  EXPECT_FALSE(peek_unsigned_integer(".5"));
  EXPECT_FALSE(peek_unsigned_integer("x"));
  EXPECT_FALSE(peek_unsigned_integer(""));
}

// 7.3.3: a signed integer is an optional `+`/`-` followed by digits.
TEST(PdfObjectParser, read_integer) {
  EXPECT_EQ(read_integer("123"), 123);
  EXPECT_EQ(read_integer("-5"), -5);
  EXPECT_EQ(read_integer("+7"), 7);
  EXPECT_EQ(read_integer("0"), 0);
}

// skip_past advances just past the first occurrence of the marker and reports
// whether it was found; on a miss it consumes the whole stream.
TEST(PdfObjectParser, skip_past) {
  using Result = std::pair<bool, std::string>;

  // cursor lands immediately after the marker
  EXPECT_EQ(skip_past("hello world", "world"), Result(true, ""));
  EXPECT_EQ(skip_past("abcXYdef", "XY"), Result(true, "def"));

  // the first occurrence wins
  EXPECT_EQ(skip_past("aXYbXYc", "XY"), Result(true, "bXYc"));

  // not found: the stream is consumed to eof
  EXPECT_EQ(skip_past("abcdef", "XY"), Result(false, ""));

  // an empty marker matches immediately and consumes nothing
  EXPECT_EQ(skip_past("abc", ""), Result(true, "abc"));

  // markers with internal repetition must still match across an overlapping
  // partial match (KMP correctness): "aab" in "aaab", and the doubled-`e` /
  // doubled prefix cases for "endstream"
  EXPECT_EQ(skip_past("aaab rest", "aab"), Result(true, " rest"));
  EXPECT_EQ(skip_past("eendstream!", "endstream"), Result(true, "!"));
  EXPECT_EQ(skip_past("<<...>>stream\nbytes\nendstreamX", "endstream"),
            Result(true, "X"));
}

// 7.3.4.3: a hex string is the bytes of its hex digits, with whitespace ignored
// and an odd final digit assumed to be followed by a 0.
TEST(PdfObjectParser, hex_string_basic) {
  EXPECT_EQ(read_hex_string("<48656C6C6F>"), "Hello");
  EXPECT_EQ(read_hex_string("<>"), "");
}

TEST(PdfObjectParser, hex_string_ignores_whitespace) {
  EXPECT_EQ(read_hex_string("<48 65\n6c\t6C 6F>"), "Hello");
  EXPECT_EQ(read_hex_string("< 48 >"), "H");
}

TEST(PdfObjectParser, hex_string_pads_odd_digit) {
  // trailing "7" is read as "70"
  EXPECT_EQ(read_hex_string("<48656C6C6F7>"), "Hellop");
  EXPECT_EQ(read_hex_string("<4>"), "\x40");
  // whitespace before the terminator must not be mistaken for a digit
  EXPECT_EQ(read_hex_string("<41 >"), "A");
}
