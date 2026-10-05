#include <odr/internal/common/text_cursor.hpp>

#include <gtest/gtest.h>

#include <stdexcept>
#include <string_view>

using namespace odr::internal;

TEST(TextCursor, rejects_overread_and_distinguishes_nul_from_end) {
  TextCursor cursor(std::string_view("\0x", 2));
  EXPECT_THROW(cursor.advance(3), std::out_of_range);
  EXPECT_EQ(cursor.rest().size(), 2);
  EXPECT_TRUE(cursor.consume('\0'));
  EXPECT_EQ(cursor.take(), 'x');
  EXPECT_FALSE(cursor.consume('\0'));
  EXPECT_TRUE(cursor.empty());
  EXPECT_NO_THROW(cursor.advance(0));
  EXPECT_THROW(cursor.advance(1), std::out_of_range);
}
