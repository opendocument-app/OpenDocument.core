#include <odr/internal/pdf/pdf_object_parser.hpp>

#include <sstream>
#include <string>

#include <gtest/gtest.h>

using namespace odr::internal::pdf;

namespace {
std::string read_hex_string(const std::string &input) {
  std::istringstream in(input);
  ObjectParser parser(in);
  return std::get<HexString>(parser.read_string()).string;
}
} // namespace

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
