#include <odr/internal/font/type1_charstring.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

using namespace odr::internal::font::type1;

namespace {

/// Encode an integer in the Type1/Type2 shared number forms (no 28/255 needed
/// for the small values used here).
void num(std::string &s, const std::int32_t v) {
  if (v >= -107 && v <= 107) {
    s += static_cast<char>(v + 139);
  } else if (v >= 108 && v <= 1131) {
    const std::int32_t u = v - 108;
    s += static_cast<char>((u >> 8) + 247);
    s += static_cast<char>(u & 0xff);
  } else if (v >= -1131 && v <= -108) {
    const std::int32_t u = -v - 108;
    s += static_cast<char>((u >> 8) + 251);
    s += static_cast<char>(u & 0xff);
  }
}

void op(std::string &s, const std::int32_t o) { s += static_cast<char>(o); }

} // namespace

TEST(Type1CharstringTest, HsbwWidthAndSideBearing) {
  // sbx=10 wx=200 hsbw  100 0 rmoveto  50 50 rlineto  endchar
  std::string t1;
  num(t1, 10);
  num(t1, 200);
  op(t1, 13); // hsbw
  num(t1, 100);
  num(t1, 0);
  op(t1, 21); // rmoveto
  num(t1, 50);
  num(t1, 50);
  op(t1, 5);  // rlineto
  op(t1, 14); // endchar

  const std::string out = to_type2(t1, {});

  // Type2: [width 200][dx 100+sbx 10 = 110][dy 0] rmoveto  [50][50] rlineto
  //        endchar.
  std::string expected;
  num(expected, 200); // width prepended
  num(expected, 110); // 100 + side bearing 10
  num(expected, 0);
  op(expected, 21); // rmoveto
  num(expected, 50);
  num(expected, 50);
  op(expected, 5);  // rlineto
  op(expected, 14); // endchar
  EXPECT_EQ(out, expected);
}

TEST(Type1CharstringTest, FlattensCallSubr) {
  // subr 0: 50 50 rlineto return
  std::string subr0;
  num(subr0, 50);
  num(subr0, 50);
  op(subr0, 5);   // rlineto
  op(subr0, 11);  // return
  op(subr0, 255); // Unreachable truncated operand.

  // 0 0 hsbw  0 0 rmoveto  0 callsubr  endchar
  std::string t1;
  num(t1, 0);
  num(t1, 0);
  op(t1, 13); // hsbw
  num(t1, 0);
  num(t1, 0);
  op(t1, 21); // rmoveto
  num(t1, 0);
  op(t1, 10); // callsubr 0
  op(t1, 14); // endchar

  const std::array<std::string, 1> subrs = {subr0};
  const std::string out = to_type2(t1, subrs);

  // The subr's rlineto is inlined; expect width(0) rmoveto, then rlineto, then
  // endchar.
  std::string expected;
  num(expected, 0); // width
  num(expected, 0);
  num(expected, 0);
  op(expected, 21); // rmoveto
  num(expected, 50);
  num(expected, 50);
  op(expected, 5);  // rlineto (from subr)
  op(expected, 14); // endchar
  EXPECT_EQ(out, expected);
}

TEST(Type1CharstringTest, FoldsDiv) {
  // 0 0 hsbw  600 2 div 0 rmoveto  endchar  -> dx = 300
  std::string t1;
  num(t1, 0);
  num(t1, 0);
  op(t1, 13); // hsbw
  num(t1, 600);
  num(t1, 2);
  t1 += static_cast<char>(12);
  t1 += static_cast<char>(12); // div
  num(t1, 0);
  op(t1, 21); // rmoveto
  op(t1, 14); // endchar

  const std::string out = to_type2(t1, {});
  std::string expected;
  num(expected, 0);   // width
  num(expected, 300); // 600 / 2
  num(expected, 0);
  op(expected, 21); // rmoveto
  op(expected, 14); // endchar
  EXPECT_EQ(out, expected);
}

TEST(Type1CharstringTest, PreservesFractionalWidthAndBothSideBearings) {
  for (const std::int32_t move : {21, 22, 4}) {
    std::string input;
    for (const std::int32_t value : {10, 20, 201, 2}) {
      num(input, value);
    }
    input += std::string("\x0c\x0c", 2); // width = 201 / 2
    num(input, 0);
    input += std::string("\x0c\x07", 2); // sbw
    num(input, 30);
    if (move == 21) {
      num(input, 40);
    }
    op(input, move);
    num(input, 5);
    num(input, 10);
    op(input,
       1); // A Type1 stem after a path cannot be copied as a Type2 hstem.
    op(input, 14);
    std::string expected("\xff\0\x64\x80\0", 5); // 100.5 in 16.16
    num(expected, move == 4 ? 10 : 40);
    num(expected, move == 4 ? 50 : move == 22 ? 20 : 60);
    op(expected, 21);
    op(expected, 14);
    EXPECT_EQ(to_type2(input, {}), expected);
  }
  std::string incomplete;
  num(incomplete, 0);
  num(incomplete, 200);
  op(incomplete, 13);
  std::string expected;
  num(expected, 200);
  op(expected, 14);
  EXPECT_EQ(to_type2(incomplete, {}), expected);
}

TEST(Type1CharstringTest, RejectsInvalidArithmeticStacksAndSubroutines) {
  EXPECT_THROW((void)to_type2(std::string(25, static_cast<char>(139)), {}),
               std::runtime_error);
  EXPECT_THROW((void)to_type2(std::string("\x8c\x8b\x0c\x0c", 4), {}),
               std::runtime_error);
  EXPECT_THROW((void)to_type2(std::string("\xff\0\0\x9c\x40\x16", 6), {}),
               std::runtime_error);
  const std::array<std::string, 1> subrs{std::string("\x8b\x0a", 2)};
  EXPECT_THROW((void)to_type2(subrs[0], subrs), std::runtime_error);
  EXPECT_THROW((void)to_type2(std::string("\x8c\x8d\x0c\x0c\x0a", 5), subrs),
               std::runtime_error);
}
