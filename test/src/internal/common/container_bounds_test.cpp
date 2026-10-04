#include <odr/internal/common/element_registry.hpp>
#include <odr/internal/common/text_cursor.hpp>

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
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

TEST(SortedSideTable, rejects_wide_ids_without_corrupting_order) {
  SortedSideTable<std::uint32_t, std::uint8_t> table;
  table.emplace(1, 10);
  table.emplace(255, 20);
  EXPECT_THROW(table.emplace(256, 30), std::out_of_range);
  EXPECT_THROW(
      table.emplace(std::numeric_limits<odr::ElementIdentifier>::max(), 40),
      std::out_of_range);
  EXPECT_EQ(table.at(1), 10);
  EXPECT_EQ(table.at(255), 20);
  EXPECT_EQ(table.find(256), nullptr);
  EXPECT_EQ(table.find(0), nullptr);
}
