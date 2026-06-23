#include <odr/internal/font/type1_charstring.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

using namespace odr::internal::font::type1;

namespace {

/// Encode an integer in the Type1/Type2 shared number forms (no 28/255 needed
/// for the small values used here).
void num(std::string &s, const int v) {
  if (v >= -107 && v <= 107) {
    s += static_cast<char>(v + 139);
  } else if (v >= 108 && v <= 1131) {
    const int u = v - 108;
    s += static_cast<char>((u >> 8) + 247);
    s += static_cast<char>(u & 0xff);
  } else if (v >= -1131 && v <= -108) {
    const int u = -v - 108;
    s += static_cast<char>((u >> 8) + 251);
    s += static_cast<char>(u & 0xff);
  }
}

void op(std::string &s, const int o) { s += static_cast<char>(o); }

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

  const Type2Charstring out = to_type2(t1, {});
  EXPECT_TRUE(out.has_width);
  EXPECT_EQ(out.width, 200);

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
  EXPECT_EQ(out.charstring, expected);
}

TEST(Type1CharstringTest, FlattensCallSubr) {
  // subr 0: 50 50 rlineto return
  std::string subr0;
  num(subr0, 50);
  num(subr0, 50);
  op(subr0, 5);  // rlineto
  op(subr0, 11); // return

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

  const Type2Charstring out = to_type2(t1, {subr0});

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
  EXPECT_EQ(out.charstring, expected);
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

  const Type2Charstring out = to_type2(t1, {});
  std::string expected;
  num(expected, 0);   // width
  num(expected, 300); // 600 / 2
  num(expected, 0);
  op(expected, 21); // rmoveto
  op(expected, 14); // endchar
  EXPECT_EQ(out.charstring, expected);
}
